#include "portfs.h"

#define PORTFS_MAGIC 0x18181818


static int portfs_fill_superblock(struct portfs_superblock *msb, const struct portfs_disk_superblock *dsb)
{
    if (!msb || !dsb)
    {
        pr_warn("portfs_fill_superblock: One of arguments was NULL");
        return -EINVAL;
    }

    msb->magic_number = be32_to_cpu(dsb->magic_number);
    msb->block_size = be32_to_cpu(dsb->block_size);
    msb->total_blocks = be32_to_cpu(dsb->total_blocks);
    msb->block_bitmap_start = be32_to_cpu(dsb->block_bitmap_start);
    msb->block_bitmap_size = be32_to_cpu(dsb->block_bitmap_size);
    msb->filetable_start = be32_to_cpu(dsb->filetable_start);
    msb->filetable_size = be32_to_cpu(dsb->filetable_size);
    msb->data_start = be32_to_cpu(dsb->data_start);
    msb->checksum = be32_to_cpu(dsb->checksum);
    msb->max_file_count = be32_to_cpu(dsb->max_file_count);
    msb->filetable = NULL;
    msb->block_bitmap = NULL;
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
        return ERR_PTR(err);
    }
    return msb;
}


static int portfs_init_filetable(struct portfs_superblock *msb)
{
    if (!msb)
    {
        pr_err("portfs_init_filetable: Argument is NULL\n");
        return -EINVAL;
    }

    loff_t offset = msb->filetable_start * msb->block_size;
    size_t total_size = msb->filetable_size * msb->block_size;
    ssize_t bytes_read = 0;
    char* buffer = NULL;

    buffer = kmalloc(total_size, GFP_KERNEL);
    if (!buffer)
    {
        pr_err("portfs_init_filetable: Could not allocate memory\n");
        return -ENOMEM;
    }

    bytes_read = kernel_read(storage_filp, buffer, total_size, &offset);
    if (bytes_read < 0)
    {
        pr_err("portfs_init_filetable: Failed to read filetable: %zd\n", bytes_read);
        kfree(buffer);
        return bytes_read;
    }
    if (bytes_read != total_size)
    {
        pr_err("portfs_init_filetable: Incomplete read of filetable: expected %zu, got %zd\n",
               total_size, bytes_read);
        kfree(buffer);
        return -EIO;
    }

    msb->filetable = kmalloc(total_size, GFP_KERNEL);
    if (!(msb->filetable))
    {
        pr_err("portfs_init_filetable: Could not allocate memory\n");
        kfree(buffer);
        return -ENOMEM;
    }

    for (int i = 0; i < msb->max_file_count; ++i)
    {
        memcpy(&msb->filetable[i],
               buffer + i * sizeof(struct filetable_entry),
               sizeof(struct filetable_entry));
    }
    kfree(buffer);
    pr_info("portfs_init_filetable: Filetable read successfully\n");
    return 0;
}


static int portfs_init_block_bitmap(struct portfs_superblock * msb)
{
    if (!msb)
    {
        pr_err("portfs_init_filetable: Argument is NULL\n");
        return -EINVAL;
    }

    loff_t offset = msb->block_bitmap_start * msb->block_size;
    size_t block_bitmap_size = msb->block_bitmap_size * msb->block_size;

    msb->block_bitmap = kmalloc(block_bitmap_size, GFP_KERNEL);
    if (!(msb->block_bitmap))
    {
        pr_err("portfs_init_block_bitmap: Could not allocate memory\n");
        return -ENOMEM;
    }

    ssize_t bytes_read = kernel_read(storage_filp,
                                     msb->block_bitmap,
                                     block_bitmap_size,
                                     &offset);
    if (bytes_read < 0)
    {
        kfree(msb->block_bitmap);
        return bytes_read;
    }
    return 0;
}


static int portfs_init_fs_data(void *storage_path)
{
    pr_info("Initializing portfs service data\n");
    char *path = storage_path;
    strncpy(storage_path, path, MAX_STORAGE_PATH - 1);
    pr_info("Mounting filesystem with storage file: %s\n", storage_path);
    int err = 0;
    err = storage_init();
    if (err)
        return err;
    
    struct portfs_superblock *msb = portfs_init_superblock();
    if (IS_ERR(msb))
    {
        // free resources
    }
    err = portfs_init_filetable(msb);
    if (err)
    {
        // free resources
    }
    err = portfs_init_block_bitmap(msb);
    if (err)
    {
        // free resources
    }
    return 0;
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
    sb->s_magic = PORTFS_MAGIC;
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
