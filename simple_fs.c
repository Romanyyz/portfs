#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#define BLOCKSIZE 4096
#define GROUPSIZE 8 * BLOCKSIZE

#define FILE_PATH "/storage/storage.sfs"
#define FILE_SIZE (4*1024*1024*1024)

static struct file* filp = NULL;


static int simplefs_create(struct mnt_idmap *idmap, struct inode *dir,
                           struct dentry *dentry, umode_t mode, bool excl);
static int simplefs_unlink(struct inode *dir, struct dentry *dentry);

static int simplefs_iterate_shared(struct file *filp, struct dir_context *ctx);


static struct file_operations fops = {
    .owner = THIS_MODULE,
    .iterate_shared = simplefs_iterate_shared, // open via ls
};

static struct inode_operations inode_ops = {
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
    char* name;
    char* data;
    char* offset;
    int size;
};

struct filetable_entry
{
    int fileStartBlock;
    int sizeInBlocks;
    char name[64];
};

static int __init simplefs_init(void)
{
    umode_t mode = S_IRUSR | S_IWUSR;
    filp = filp_open(FILE_PATH, O_RDWR | O_CREAT | O_EXCL, mode);
    if (IS_ERR(filp))
    {
        pr_err("Failed to create file\n");
        return PTR_ERR(filp);
    }

    vfs_truncate(&filp->f_path, FILE_SIZE);


    pr_info("Simple-fs loaded.\n");
    return 0;
}

static void __exit simplefs_exit(void)
{
    filp_close(filp, NULL);
    pr_info("Simple-fs unloaded.\n");
}

module_init(simplefs_init);
module_exit(simplefs_exit);

MODULE_LICENSE("GPL");
