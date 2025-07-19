#ifndef PORTFS_H
#define PORTFS_H

#include <linux/kernel.h>
#include <linux/module.h>
#include "linux/fs.h"

#include "shared_structs.h"

static inline const struct extent *get_extent(const struct filetable_entry *fe, size_t i)
{
    return (i < DIRECT_EXTENTS)
        ? &fe->direct_extents[i]
        : &fe->indirect_extents[i - DIRECT_EXTENTS];
}

static struct dentry *portfs_mount(struct file_system_type *fs_type,
                                   int flags, const char *dev_name, void *data);
struct file* portfs_storage_init(char *path);

static struct file_system_type portfs_type = {
    .owner = THIS_MODULE,
    .name = "portfs",
    .mount = portfs_mount,
    .kill_sb = generic_shutdown_super,
    .fs_flags = FS_USERNS_MOUNT,
};

extern struct file* storage_filp;

#endif // PORTFS_H
