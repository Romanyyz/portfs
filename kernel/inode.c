#include "inode.h"

#include "file.h"
#include "portfs.h"
#include "shared_structs.h"
#include "bitmap.h"

static struct filetable_entry *portfs_find_free_file_entry(struct super_block *sb)
{
    struct portfs_superblock *psb = sb->s_fs_info;
    struct filetable_entry *entry = psb->filetable;

    for ( int i = 0; i < psb->max_file_count; ++i, ++entry)
    {
        if (entry->name[0] == '\0')
        {
            return entry;
        }
    }
    return NULL;
}


static struct inode *portfs_get_inode_by_name(struct super_block *sb, const char *name)
{
    struct portfs_superblock *psb = sb->s_fs_info;

    struct filetable_entry *file_entry = (struct filetable_entry *)psb->filetable;
    for (size_t i = 0; i < psb->max_file_count; ++i, ++file_entry)
    {
        if (strcmp(file_entry->name, name) == 0)
        {
            struct inode *inode = iget_locked(sb, file_entry->ino);
            if (!inode)
            {
                pr_err("portfs_get_inode_by_name: Failed to get new inode\n");
                return ERR_PTR(-ENOENT);
            }
            if (inode->i_state & I_NEW)
            {
                inode->i_mode = S_IFREG | 0644; // -rw-r--r--
                inode->i_uid = current_fsuid();
                inode->i_gid = current_fsgid();
                inode->i_size = file_entry->sizeInBytes;
                inode->i_sb = sb;

                time64_t now = ktime_get_real_seconds();
                inode->i_atime_sec = now;
                inode->i_mtime_sec = now;
                inode->i_ctime_sec = now;

                inode->i_op = &portfs_file_inode_operations;
                inode->i_fop = &portfs_file_operations;
                inode->i_private = file_entry;

                insert_inode_hash(inode);
                unlock_new_inode(inode);
            }
            return inode;
        }
    }
    return NULL;
}

static int portfs_create(struct mnt_idmap *idmap, struct inode *dir,
                         struct dentry *dentry, umode_t mode, bool excl)
{
    struct inode *inode;
    struct filetable_entry *file_entry;
    struct super_block *sb = dir->i_sb;

    if (!S_ISREG(mode))
    {
        return -EOPNOTSUPP;
    }

    pr_info("portfs_create: Creating file '%s'\n", dentry->d_name.name);

    if (dentry->d_inode)
        return -EEXIST;

    inode = iget_locked(sb, iunique(sb, 2)); // root inode is 1 so start from 2
    if (!inode)
        return -ENOMEM;
    if (inode->i_state & I_NEW)
    {
        inode->i_mode = S_IFREG | mode;
        inode->i_uid = current_fsuid();
        inode->i_gid = current_fsgid();
        inode->i_size = 0;
        inode->i_sb = sb;

        time64_t now = ktime_get_real_seconds();
        inode->i_atime_sec = now;
        inode->i_mtime_sec = now;
        inode->i_ctime_sec = now;

        inode->i_op = &portfs_file_inode_operations;
        inode->i_fop = &portfs_file_operations;

        unlock_new_inode(inode);
    }

    file_entry = portfs_find_free_file_entry(sb);
    if (file_entry == NULL)
        return -ENOSPC;

    memcpy(file_entry->name, dentry->d_name.name, sizeof(file_entry->name) - 1);
    file_entry->ino = inode->i_ino;

    inode->i_private = file_entry;

    d_instantiate(dentry, inode);

    return 0;
}

static int portfs_unlink(struct inode *dir, struct dentry *dentry)
{
    if (!dentry->d_inode)
        return -ENOENT;

    pr_info("portfs_unlink: Removing file '%s'\n", dentry->d_name.name);
    struct inode *inode = dentry->d_inode;
    struct filetable_entry *file_entry;
    struct super_block *sb = dir->i_sb;
    struct portfs_superblock *psb = sb->s_fs_info;

    file_entry = inode->i_private;
    if (!file_entry)
        return -EINVAL;

    for (size_t i = 0; i < file_entry->extentCount; ++i)
    {
        clear_blocks_allocated(psb->block_bitmap,
                               file_entry->extents[i].start_block,
                               file_entry->extents[i].length);
    }
    memset(file_entry, 0, sizeof(*file_entry));

    time64_t now = ktime_get_real_seconds();
    dir->i_ctime_sec = dir->i_mtime_sec = now;

    inode->i_private = NULL;

    clear_nlink(inode);
    mark_inode_dirty(inode);

    return 0;
}


static struct dentry *portfs_lookup(struct inode *dir, struct dentry *dentry,
                                    unsigned int flags)
{
    if (!dentry || !dentry->d_name.name)
    {
        pr_err("portfs_lookup: NULL dentry or d_name.name\n");
        return ERR_PTR(-EINVAL);
    }

    struct super_block *sb = dir->i_sb;
    struct inode *inode = NULL;

    if (dentry->d_name.len == 0 || dentry->d_name.len > NAME_MAX)
    {
        return ERR_PTR(-ENAMETOOLONG);
    }

    inode = portfs_get_inode_by_name(sb, dentry->d_name.name);
    if (inode)
    {
        d_splice_alias(inode, dentry);
    }
    else
    {
        d_add(dentry, NULL);
    }

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
