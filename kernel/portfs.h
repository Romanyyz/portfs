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


static struct dentry *portfs_mount(struct file_system_type *fs_type,
                                   int flags, const char *dev_name, void *data);
struct file* portfs_storage_init(char *path);

static struct file_system_type portfs_type = {
    .owner = THIS_MODULE,
    .name = "portfs",
    .mount = portfs_mount,
    .kill_sb = kill_litter_super,
    .fs_flags = FS_USERNS_MOUNT,
};

extern struct file* storage_filp;

#endif // PORTFS_H
