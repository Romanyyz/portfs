 #include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>

#define BLOCKSIZE 4096
#define GROUPSIZE 8 * BLOCKSIZE


static int simplefs_create(struct mnt_idmap *idmap, struct inode *dir,
                           struct dentry *dentry, umode_t mode, bool excl);
static int simplefs_unlink(struct inode *dir, struct dentry *dentry);

static int simplefs_iterate_shared(struct file *filp, struct dir_context *ctx);


static struct file_operations fops = {
    .owner = THIS_MODULE,
    .iterate_shared = simplefs_iterate_shared, // open via ls
};

static struct inode_operations myfs_inode_ops = {
    .create = simplefs_create,
    .unlink = simplefs_unlink,
};

static int simplefs_create(struct mnt_idmap *idmap, struct inode *dir,
                           struct dentry *dentry, umode_t mode, bool excl)
{
    return 0;
    // Create new file
}

static int simplefs_unlink(struct inode *dir, struct dentry *dentry)
{
    return 0;
    // Delete file
}

static int simplefs_iterate_shared(struct file *filp, struct dir_context *ctx)
{
    return 0;
    // Read filesystem content
}

struct simplefs_file
{
    char *name;
    char *data;
    int size;
};

static int __init simplefs_init(void)
{


    pr_info("Simple-fs loaded.\n");
    return 0;
}

static void __exit simplefs_exit(void)
{
    pr_info("Simple-fs unloaded.\n");
}

module_init(simplefs_init);
module_exit(simplefs_exit);

MODULE_LICENSE("GPL");
