#include "portfs.h"

static int portfs_fill_super(struct super_block *sb, void *data, int silent)
{
    struct inode *root_inode;

    sb->s_magic = MYFS_MAGIC;
    //sb->s_op = &portfs_super_operations;
    char *path = data;
    strncpy(storage_path, path, MAX_STORAGE_PATH - 1);
    pr_info("Mounting filesystem with storage file: %s\n", storage_path);
    int err = storage_init();
    if (err)
        return err;

    root_inode = new_inode(sb);
    if (!root_inode)
    {
        filp_close(storage_filp, NULL);
        return -ENOMEM;
    }

    inode_init_owner(&nop_mnt_idmap, root_inode, NULL, S_IFDIR | 0755);
    root_inode->i_ino = 1;
    root_inode->i_mode = S_IFDIR | 0755;
    root_inode->i_sb = sb;

    sb->s_root = d_make_root(root_inode);
    if (!sb->s_root) {
        iput(root_inode);
        filp_close(storage_filp, NULL);
        return -ENOMEM;
    }
    return 0;
}

static struct dentry *portfs_mount(struct file_system_type *fs_type,
    int flags, const char *dev_name, void *data)
{
    return mount_nodev(fs_type, flags, data, portfs_fill_super);
}

static int __init portfs_init(void)
{
    int err = register_filesystem(&portfs_type);
    if (err)
    {
        pr_err("Failed to register portfs filesystem\n");
        return err;
    }

    pr_info("portfs loaded.\n");
    return 0;
}

static void __exit portfs_exit(void)
{
    unregister_filesystem(&portfs_type);
    filp_close(storage_filp, NULL);
    pr_info("portfs unloaded.\n");
}

module_init(portfs_init);
module_exit(portfs_exit);

MODULE_LICENSE("GPL");
