#include "portfs.h"

static int portfs_create(struct mnt_idmap *idmap, struct inode *dir,
                         struct dentry *dentry, umode_t mode, bool excl)
{
    // Create new file
    return 0;
}

static int portfsfs_unlink(struct inode *dir, struct dentry *dentry)
{
    // Delete file
    return 0;
}

static int portfs_iterate_shared(struct file *filp, struct dir_context *ctx)
{
    // Read filesystem content
    return 0;
}
