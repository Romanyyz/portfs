#include "inode.h"

#include "linux/err.h"
#include "linux/fs.h"
#include "linux/stat.h"

#include "file.h"
#include "portfs.h"
#include "shared_structs.h"
#include "block_bitmap.h"
#include "extent_alloc.h"
#include "directory.h"

/*
static uint32_t portfs_alloc_ino(struct portfs_superblock *psb)
{
    uint8_t *inode_bitmap = psb->ino_bitmap;
}
*/


static void portfs_free_extent(struct portfs_superblock *psb, struct extent *extent)
{
    uint8_t *block_bitmap = psb->block_bitmap;
    clear_blocks_allocated(block_bitmap, extent->start_block, extent->length);
    extent->start_block = 0;
    extent->length = 0;
}


struct filetable_entry *portfs_find_free_file_entry(struct portfs_superblock *psb)
{
    if (!psb)
        return NULL;

    struct filetable_entry *entry = psb->filetable;

    for ( int i = 0; i < psb->max_file_count; ++i, ++entry)
    {
        if (entry->mode == 0)
        {
            return entry;
        }
    }
    return NULL;
}


struct inode *portfs_get_inode_by_number(struct super_block *sb, uint32_t ino)
{
    struct portfs_superblock *psb = sb->s_fs_info;

    struct filetable_entry *file_entry = (struct filetable_entry *)psb->filetable;
    for (size_t i = 0; i < psb->max_file_count; ++i, ++file_entry)
    {
        if (file_entry->ino == ino)
        {
            struct inode *inode = iget_locked(sb, file_entry->ino);
            if (!inode)
            {
                pr_err("portfs_get_inode_by_name: Failed to get new inode\n");
                return ERR_PTR(-ENOENT);
            }
            if (inode->i_state & I_NEW)
            {
                inode->i_mode = file_entry->mode;
                inode->i_uid = current_fsuid();
                inode->i_gid = current_fsgid();
                inode->i_size = file_entry->size_in_bytes;
                inode->i_sb = sb;

                time64_t now = ktime_get_real_seconds();
                inode->i_atime_sec = now;
                inode->i_mtime_sec = now;
                inode->i_ctime_sec = now;

                if (S_ISREG(file_entry->mode))
                {
                    inode->i_op = &portfs_file_inode_operations;
                    inode->i_fop = &portfs_file_operations;
                }
                else if (S_ISDIR(file_entry->mode))
                {
                    inode->i_op = &portfs_dir_inode_operations;
                    inode->i_fop = &portfs_dir_file_operations;
                }
                inode->i_private = file_entry;

                insert_inode_hash(inode);
                unlock_new_inode(inode);
            }
            return inode;
        }
    }
    return NULL;
}


struct inode *portfs_make_inode(struct super_block *sb, umode_t mode)
{
    struct inode *inode = iget_locked(sb, iunique(sb, 2)); // root inode is 1 so start from 2
    if (IS_ERR(inode))
        return inode;

    if (!(inode->i_state & I_NEW))
    {
        pr_err("portfs_make_inode: Inode %lu not I_NEW after iget_locked!\n", inode->i_ino);
        clear_nlink(inode);
        iput(inode);
        return ERR_PTR(-EEXIST);
    }
    pr_info("portfs_make_inode: Inode %p created. i_state: %x, I_NEW set: %d (after new_inode)\n",
                    inode, inode->i_state, (inode->i_state & I_NEW) ? 1 : 0);

    inode->i_mode = mode;
    inode->i_uid = current_fsuid();
    inode->i_gid = current_fsgid();
    inode->i_size = 0;
    inode->i_sb = sb;
    inode->i_blocks = 0;

    time64_t now = ktime_get_real_seconds();
    inode->i_atime_sec = now;
    inode->i_mtime_sec = now;
    inode->i_ctime_sec = now;

    pr_info("portfs_make_inode: Inode %p FINAL CHECK. i_state: %x, I_NEW set: %d (before return)\n",
                inode, inode->i_state, (inode->i_state & I_NEW) ? 1 : 0);
    return inode;
}


static int portfs_create(struct mnt_idmap *idmap, struct inode *dir,
                         struct dentry *dentry, umode_t mode, bool excl)
{
    pr_info("portfs_create: Creating file '%s'\n", dentry->d_name.name);

    // Might be unnecessary cuz kernel should have already checked this
    if (dentry->d_inode)
        return -EEXIST;

    struct filetable_entry *file_entry;
    struct super_block *sb = dir->i_sb;

    struct inode *inode = portfs_make_inode(sb, mode);
    if (IS_ERR(inode))
    {
        pr_err("portfs_create: Failed to create inode for %s\n", dentry->d_name.name);
        return PTR_ERR(inode);
    }

    struct portfs_superblock *psb = sb->s_fs_info;
    file_entry = portfs_find_free_file_entry(psb);
    if (file_entry == NULL)
    {
        clear_nlink(inode);
        iput(inode);
        return -ENOSPC;
    }

    inode_init_owner(idmap, inode, dir, mode);
    inode->i_private = file_entry;
    inode->i_op = &portfs_file_inode_operations;
    inode->i_fop = &portfs_file_operations;

    file_entry->mode = inode->i_mode;
    file_entry->ino = inode->i_ino;
    file_entry->size_in_bytes = 0;

    struct filetable_entry *parent_dir = dir->i_private;
    struct dir_entry d_entry;
    strncpy(d_entry.name, dentry->d_name.name, sizeof(d_entry.name));
    d_entry.inode_number = file_entry->ino;

    int err = portfs_de_add(psb, parent_dir, &d_entry);
    if (err)
    {
        clear_nlink(inode);
        iput(inode);
        return err;
    }

    d_instantiate_new(dentry, inode);
    mark_inode_dirty(inode);
    mark_inode_dirty(dir);

    return 0;
}

struct dentry *portfs_mkdir(struct mnt_idmap *idmap, struct inode *dir,
				            struct dentry *dentry, umode_t mode)
{
    pr_info("portfs_mkdir: Creating directory '%s'\n", dentry->d_name.name);
    pr_info("portfs_mkdir: dentry passed: %p, dentry->d_inode: %p\n", dentry, dentry->d_inode);

    // Might be unnecessary cuz kernel should have checked this already
    if (dentry->d_inode)
    {
        pr_warn("portfs_mkdir: dentry already has an inode! '%s'\n", dentry->d_name.name);
        return ERR_PTR(-EEXIST);
    }

    struct filetable_entry *file_entry;
    struct super_block *sb = dir->i_sb;

    struct inode *inode = portfs_make_inode(sb, mode);
    if (IS_ERR(inode))
        return ERR_CAST(inode);

    pr_info("portfs_mkdir: Received inode from make_inode: %p, ino: %lu\n", inode, inode->i_ino);

    struct portfs_superblock *psb = sb->s_fs_info;
    file_entry = portfs_find_free_file_entry(psb);
    if (file_entry == NULL)
    {
        clear_nlink(inode);
        iput(inode);
        return ERR_PTR(-ENOSPC);
    }

    set_nlink(inode, 2);
    inode_init_owner(idmap, inode, dir, S_IFDIR | mode);
    inode->i_private = file_entry;
    inode->i_op = &portfs_dir_inode_operations;
    inode->i_fop = &portfs_dir_file_operations;

    file_entry->mode = inode->i_mode;
    file_entry->ino = inode->i_ino;
    file_entry->size_in_bytes = 0;
    file_entry->dir.parent_dir_ino = dir->i_ino;

    struct filetable_entry *parent_dir = dir->i_private;
    struct dir_entry d_entry;
    strncpy(d_entry.name, dentry->d_name.name, sizeof(d_entry.name));
    d_entry.inode_number = file_entry->ino;

    int err = portfs_de_add(psb, parent_dir, &d_entry);
    if (err)
    {
        clear_nlink(inode);
        iput(inode);
        return ERR_PTR(err);
    }

    d_instantiate_new(dentry, inode);

    mark_inode_dirty(inode);
    mark_inode_dirty(dir);

    return NULL;
}


static int portfs_rmdir(struct inode *parent_dir, struct dentry *dentry)
{
    pr_info("portfs_rmdir: Removing directory '%s'\n", dentry->d_name.name);
    struct inode *inode = dentry->d_inode;
    struct portfs_superblock *psb = inode->i_sb->s_fs_info;

    if (!inode || !S_ISDIR(inode->i_mode))
    {
        pr_err("portfs_rmdir: Invalid inode or not a directory\n");
        return -ENOTDIR;
    }

    struct filetable_entry *dir = inode->i_private;
    if (!portfs_is_dir_empty(psb, dir))
    {
        pr_err("portfs_rmdir: Directory not empty\n");
        return -ENOTEMPTY;
    }

    if (dir->dir.dir_block != 0)
        clear_block_allocated(psb->block_bitmap, dir->dir.dir_block);

    if (dir->dir_entries)
        kfree(dir->dir_entries);

    memset(dir, 0, sizeof(*dir));

    portfs_de_remove(psb, parent_dir->i_private, dentry->d_name.name);

    time64_t now = ktime_get_real_seconds();
    parent_dir->i_ctime_sec = parent_dir->i_mtime_sec = now;

    inode->i_private = NULL;

    clear_nlink(inode);
    mark_inode_dirty(inode);
    mark_inode_dirty(parent_dir);

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

    for (size_t i = 0; i < file_entry->file.extent_count; ++i)
    {
        const struct extent *ext = get_extent(file_entry, i);
        clear_blocks_allocated(psb->block_bitmap,
                               ext->start_block,
                               ext->length);
    }
    memset(file_entry, 0, sizeof(*file_entry));

    portfs_de_remove(psb, dir->i_private, dentry->d_name.name);

    time64_t now = ktime_get_real_seconds();
    dir->i_ctime_sec = dir->i_mtime_sec = now;

    inode->i_private = NULL;

    clear_nlink(inode);

    mark_inode_dirty(inode);
    mark_inode_dirty(dir);

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

    if (dentry->d_name.len == 0 || dentry->d_name.len > MAX_NAME_LENGTH)
    {
        return ERR_PTR(-ENAMETOOLONG);
    }

    struct portfs_superblock *psb = sb->s_fs_info;
    struct filetable_entry *parent_dir = dir->i_private;
    struct dir_entry *d_entry = portfs_de_find(psb, parent_dir, dentry->d_name.name);
    if (IS_ERR(d_entry))
        return dentry;

    if (d_entry == NULL)
    {
        d_add(dentry, NULL);
        return NULL;
    }

    struct inode *inode = NULL;
    inode = portfs_get_inode_by_number(sb, d_entry->inode_number);
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
    struct inode *inode = d_inode(path->dentry);
    generic_fillattr(idmap, request_mask, inode, stat);
    return 0;
}


static int portfs_truncate(struct inode *inode, loff_t new_size)
{
    pr_info("portfs_truncate: Beginning");
    struct filetable_entry *file_entry = inode->i_private;
    struct portfs_superblock *psb = inode->i_sb->s_fs_info;
    if (new_size == 0) {
        for (int i = 0; i < file_entry->file.extent_count; ++i)
            portfs_free_extent(psb, &file_entry->file.direct_extents[i]);

        file_entry->file.extent_count = 0;
        file_entry->size_in_bytes = 0;
        inode->i_size = 0;

        pr_info("portfs_truncate: Truncated to 0");

        return 0;
    }

    loff_t old_blocks = (file_entry->size_in_bytes + psb->block_size - 1) / psb->block_size;
    loff_t new_blocks = (new_size + psb->block_size - 1) / psb->block_size;
    loff_t blocks_to_remove = old_blocks - new_blocks;
    if (blocks_to_remove == 0)
        return 0;

    for (int i = file_entry->file.extent_count - 1; i >= 0; ++i)
    {
        if (blocks_to_remove >= file_entry->file.direct_extents[i].length)
        {
            blocks_to_remove -= file_entry->file.direct_extents[i].length;
            portfs_free_extent(psb, &file_entry->file.direct_extents[i]);
            file_entry->file.extent_count--;
        }
        else
        {
            break;
        }
    }

    file_entry->size_in_bytes = new_size;
    inode->i_size = new_size;
    if (blocks_to_remove != 0)
    {
        pr_warn("portfs_truncate: Failed to free %lld blocks", blocks_to_remove);
        return -EIO;
    }

    return 0;
}

static int portfs_extend(struct inode *inode, loff_t new_size)
{
    pr_info("portfs_extend: Beginning");
    struct filetable_entry *file_entry = inode->i_private;
    struct portfs_superblock *psb = inode->i_sb->s_fs_info;
    size_t needed_size = new_size - file_entry->size_in_bytes;
    const size_t available_size = portfs_get_allocated_size(file_entry, psb->block_size)
                                    - file_entry->size_in_bytes;

    if (available_size >= needed_size)
    {
        inode->i_size = new_size;
        file_entry->size_in_bytes = new_size;
        return 0;
    }

    int err = portfs_allocate_memory(psb, file_entry, needed_size);
    if (err)
    {
        pr_err("portfs_extend: Failed to allocate %ld bytes", needed_size);
        return err;
    }

    inode->i_size = new_size;
    file_entry->size_in_bytes = new_size;
    return 0;
}


static int portfs_setattr(struct mnt_idmap *idmap, struct dentry *dentry,
                          struct iattr *attr)
{
    pr_info("portfs_setattr: Beginning");
    struct inode *inode = d_inode(dentry);
    int err;

    err = setattr_prepare(idmap, dentry, attr);
    if (err)
        return err;

    if (attr->ia_valid & ATTR_SIZE)
    {
        loff_t new_size = attr->ia_size;
        pr_info("portfs_setattr: old_size = %lld, new_size = %lld", inode->i_size, new_size);
        if (new_size < inode->i_size)
        {
            err = portfs_truncate(inode, new_size);
            if (err)
                return err;
        }
        else if (new_size > inode->i_size)
        {
            err = portfs_extend(inode, new_size);
            if (err)
                return err;
        }
    }
    setattr_copy(idmap, inode, attr);
    mark_inode_dirty(inode);

    pr_info("portfs_setattr: Exiting");
    return 0;
}


const struct inode_operations portfs_dir_inode_operations = {
    .create         = portfs_create,
    .unlink         = portfs_unlink,
    .lookup         = portfs_lookup,
    .mkdir          = portfs_mkdir,
    .rmdir          = portfs_rmdir,
};

const struct inode_operations portfs_file_inode_operations = {
    .getattr = portfs_getattr,
    .setattr = portfs_setattr,
};
