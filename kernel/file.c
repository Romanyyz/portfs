#include "file.h"

#include "portfs.h"


static int portfs_iterate_shared(struct file *filp, struct dir_context *ctx)
{
    // Read filesystem content
    return 0;
}


static int portfs_file_open(struct inode *inode, struct file *filp)
{
    return 0;
}


static int portfs_release_file(struct inode *inode, struct file *filp)
{
    return 0;
}


static ssize_t portfs_file_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
    return 0;
}


static ssize_t portfs_file_write(struct file *file, const char __user *buf, size_t count, loff_t *pos)
{
    return 0;
}


const struct file_operations portfs_dir_file_operations = {
    .iterate_shared = portfs_iterate_shared,
};


const struct file_operations portfs_file_operations = {
    .open    = portfs_file_open,
    .release = portfs_release_file,
    .read    = portfs_file_read,
    .write   = portfs_file_write,
};
