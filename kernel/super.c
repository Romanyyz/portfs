#include "portfs.h"

static int portfs_fill_superblock(struct portfs_superblock *msb, struct const portfs_disk_superblock *dsb)
{
    if (!msb || !dsb)
    {
        pr_warn("portfs_fill_superblock: One of arguments was NULL");
        return -EINVAL;
    }

    msb->block_size = be32_to_cpu(dsb->block_size);
    msb->total_blocks = be32_to_cpu(dsb->total_blocks);
    msb->bitmap_start = be32_to_cpu(dsb->bitmap_start);
    msb->bitmap_size = be32_to_cpu(dsb->bitmap_size);
    msb->filetable_start = be32_to_cpu(dsb->filetable_start);
    msb->filetable_size = be32_to_cpu(dsb->filetable_size);
    msb->data_start = be32_to_cpu(dsb->data_start);
    return 0;
}

static struct portfs_superblock *portfs_init_superblock(void)
{
    struct portfs_disk_superblock *dsb =
        (struct portfs_disk_superblock *)kmalloc(sizeof(struct portfs_disk_superblock), GFP_KERNEL);
    struct portfs_superblock *msb =
        (struct portfs_superblock *)kmalloc(sizeof(struct portfs_superblock), GFP_KERNEL);
    if (!msb || !dsb)
    {
        pr_err("portfs_init_superblock: Could not allocate memory for "
               "struct portfs_superblock or struct portfs_disk_superblock");
        return ERR_PTR(-ENOMEM);
    }
    loff_t pos = 0;
    ssize_t bytes_read = kernel_read(storage_filp,
                                     dsb,
                                     sizeof(struct portfs_disk_superblock),
                                     &pos);
    if (bytes_read < 0)
    {
        pr_err("portfs_init_superblock: Error reading storage file");
        kfree(dsb);
        kfree(msb);
        return ERR_PTR(bytes_read);
    }

    int err = portfs_fill_superblock(msb, dsb);
    if (err)
    {
        pr_err("portfs_init_superblock: Could not fill superblock");
        kfree(dsb);
        kfree(msb);
        return err;
    }
    return msb;
}

static int portfs_init_fs_data(void *storage_path)
{
    pr_info("Initializing portfs service data");
    char *path = storage_path;
    strncpy(storage_path, path, MAX_STORAGE_PATH - 1);
    pr_info("Mounting filesystem with storage file: %s\n", storage_path);
    int err = storage_init();
    if (err)
        return err;
    
    struct portfs_superblock *msb = portfs_init_superblock();
    if (IS_ERR(msb))
    {
        // free resources
    }
    // TODO: init filetable
    // TODO: init bitmap
}

static int portfs_fill_super(struct super_block *sb, void *data, int silent)
{
    struct portfs_disk_superblock *dsb = (struct portfs_disk_superblock *)kmalloc(sizeof(struct portfs_disk_superblock), GFP_KERNEL);
    if (!dsb)
    {
        pr_err("portfs_fill_super: Failed to allocate memory for portfs_disk_superblock");
        return -ENOMEM;
    }
    int err = portfs_init_fs_data(data);
    if (err)
    {
        kfree(dsb);
        return err;
    }

    struct inode *root_inode;
    sb->s_magic = MYFS_MAGIC;
    //sb->s_op = &portfs_super_operations;
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
