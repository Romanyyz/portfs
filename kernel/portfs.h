#ifndef PORTFS_H
#define PORTFS_H

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/types.h>
#include <linux/namei.h>
#include <linux/user_namespace.h>
#include <linux/vmalloc.h>


// Function prototypes
static int portfs_create(struct mnt_idmap *idmap, struct inode *dir,
                           struct dentry *dentry, umode_t mode, bool excl);
static int portfs_unlink(struct inode *dir, struct dentry *dentry);

static int portfs_iterate_shared(struct file *filp, struct dir_context *ctx);

static int portfs_fill_super(struct super_block *sb, void *data, int silent);
static struct dentry *portfs_mount(struct file_system_type *fs_type,
                                   int flags, const char *dev_name, void *data);
static int portfs_create(struct mnt_idmap *idmap, struct inode *dir,
                         struct dentry *dentry, umode_t mode, bool excl);
static int portfsfs_unlink(struct inode *dir, struct dentry *dentry);
static int portfs_iterate_shared(struct file *filp, struct dir_context *ctx);
struct file* portfs_storage_init(char *path);

static struct file_system_type portfs_type = {
    .owner = THIS_MODULE,
    .name = "portfs",
    .mount = portfs_mount,
    .kill_sb = kill_litter_super,
    .fs_flags = FS_USERNS_MOUNT,
};


#endif // PORTFS_H
