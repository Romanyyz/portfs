#include "portfs.h"

static const struct inode_operations portfs_dir_inode_operation;
static const struct inode_operations portfs_file_inode_operation;

static int portfs_create(struct mnt_idmap *idmap, struct inode *dir,
                         struct dentry *dentry, umode_t mode, bool excl)
{
    // Create new file
    return 0;
}

static int portfs_unlink(struct inode *dir, struct dentry *dentry)
{
    // Delete file
    return 0;
}



static struct dentry *portfs_lookup(struct inode *dir, struct dentry *dentry,
				                    unsigned int flags)
{}


static int portfs_getattr(struct mnt_idmap *idmap,
			              const struct path *path, struct kstat *stat,
			              u32 request_mask, unsigned int flags)
{}


static const struct inode_operations portfs_dir_inode_operation = {
    .create         = portfs_create,
    .lookup         = portfs_lookup,
    .unlink         = portfs_unlink,
};

static const struct inode_operations portfs_file_inode_operation = {
    .getattr = portfs_getattr,
};
