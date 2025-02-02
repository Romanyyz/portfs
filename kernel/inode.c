#include "inode.h"

#include "file.h"
#include "portfs.h"
#include "../common/shared_structs.h"


static inline int is_block_allocated(uint8_t *bitmap, uint32_t block)
{
    return bitmap[block / 8] & (1 << (block % 8));
}


static inline void set_block_allocated(uint8_t *bitmap, uint32_t block)
{
    bitmap[block / 8] |= (1 << (block % 8));
}


static inline void clear_block_allocated(uint8_t *bitmap, uint32_t block)
{
    bitmap[block / 8] &= ~(1 << (block % 8));
}


static inline int find_free_block(uint8_t *bitmap, uint32_t total_blocks)
{
    for (uint32_t i = 0; i < total_blocks; i++) {
        if (!is_block_allocated(bitmap, i))
        {
            return i;
        }
    }
    return -1;
}


static inline void clear_blocks_allocated(uint8_t *bitmap, uint32_t block, uint32_t length)
{
    for (uint32_t i = block; i <= length; ++i)
    {
        clear_block_allocated(bitmap, i);
    }
}


static struct filetable_entry *portfs_find_file_entry(struct super_block *sb)
{
    struct portfs_superblock *psb = sb->s_fs_info;
    struct filetable_entry *entry = psb->filetable;

    for (size_t i = 0; i < psb->max_file_count; ++i, ++entry)
    {
        if (entry->name[0] == '\0')
            return entry;
    }
    return NULL;
}


static struct inode *portfs_get_inode_by_name(struct super_block *sb, const char *name)
{
    struct portfs_superblock *psb = sb->s_fs_info;
    struct inode *inode = new_inode(sb);
    if (!inode)
    {
        pr_err("portfs_get_inode_by_name: Failed to allocate new inode\n");
        return ERR_PTR(-ENOMEM);
    }

    size_t num_entries = (psb->filetable_size - psb->filetable_start)
                         * psb->block_size / sizeof(struct filetable_entry);
    struct filetable_entry *file_entry = (struct filetable_entry *)psb->filetable;
    for (size_t i = 0; i < num_entries; ++i, ++file_entry)
    {
        if (strcmp(file_entry->name, name) == 0)
        {
            inode->i_ino = get_next_ino();
            inode->i_mode = S_IFREG | 0644; // -rw-r--r--
            inode->i_uid = current_fsuid();
            inode->i_gid = current_fsgid();
            inode->i_size = file_entry->sizeInBytes;
            inode->i_sb = sb;
            inode->i_atime = inode->i_mtime = current_time(inode);
            inode_set_ctime_current(inode);
            inode->i_op = &portfs_file_inode_operations;
            inode->i_fop = &portfs_file_operations;
            inode->i_private = file_entry;
            return inode;
        }
    }

    iput(inode);
    return NULL;
}

static int portfs_create(struct mnt_idmap *idmap, struct inode *dir,
                         struct dentry *dentry, umode_t mode, bool excl)
{
    struct inode *inode;
    struct filetable_entry *file_entry;
    struct super_block *sb = dir->i_sb;

    pr_info("portfs_create: Creating file '%s'\n", dentry->d_name.name);

    if (dentry->d_inode)
        return -EEXIST;

    inode = new_inode(sb);
    if (!inode)
        return -ENOMEM;

    inode->i_ino = get_next_ino();
    inode->i_mode = S_IFREG | mode;
    inode->i_uid = current_fsuid();
    inode->i_gid = current_fsgid();
    inode->i_size = 0;
    inode->i_sb = sb;
    inode->i_atime = inode->i_mtime = current_time(inode);
    inode_set_ctime_current(inode);
    inode->i_fop = &portfs_file_operations;

    file_entry = portfs_find_file_entry(sb);
    if (file_entry == NULL)
        return -ENOSPC;

    memcpy(file_entry->name, dentry->d_name.name, sizeof(file_entry->name) - 1);
    inode->i_private = file_entry;

    d_instantiate(dentry, inode);
    dget(dentry);

    return 0;
}

static int portfs_unlink(struct inode *dir, struct dentry *dentry)
{
    struct inode *inode;
    struct filetable_entry *file_entry;
    struct super_block *sb = dir->i_sb;
    struct portfs_superblock *psb = sb->s_fs_info;

    pr_info("portfs_create: Removing file '%s'\n", dentry->d_name.name);

    if (!dentry->d_inode)
        return -EEXIST;

    file_entry = inode->i_private;
    if (!file_entry)
        return -EINVAL;

    for (size_t i = 0; i < file_entry->extentCount; ++i)
    {
        clear_blocks_allocated(psb->block_bitmap,
                               file_entry->extents[i].startBlock,
                               file_entry->extents[i].length);
        file_entry->extents[i].startBlock = 0;
        file_entry->extents[i].length = 0;
    }
    file_entry->name[0] = '\0';
    file_entry->sizeInBytes = 0;
    file_entry->extentCount = 0;

    inode->i_private = NULL;

    dput(dentry);
    iput(inode);

    return 0;
}


static struct dentry *portfs_lookup(struct inode *dir, struct dentry *dentry,
                                    unsigned int flags)
{
    struct super_block *sb = dir->i_sb;
    struct portfs_superblock *psb = sb->s_fs_info;
    struct inode *inode = NULL;

     if (dentry->d_name.len == 0 || dentry->d_name.len > NAME_MAX) {
        return ERR_PTR(-ENAMETOOLONG);
    }

    inode = portfs_get_inode_by_name(sb, dentry->d_name.name);

    d_instantiate(dentry, inode);

    return NULL;
}


static int portfs_getattr(struct mnt_idmap *idmap,
                          const struct path *path, struct kstat *stat,
                          u32 request_mask, unsigned int flags)
{
    return 0;
}


const struct inode_operations portfs_dir_inode_operations = {
    .create         = portfs_create,
    .unlink         = portfs_unlink,
    .lookup         = portfs_lookup,
};

const struct inode_operations portfs_file_inode_operations = {
    .getattr = portfs_getattr,
};
