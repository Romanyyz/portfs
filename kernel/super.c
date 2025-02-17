#include "portfs.h"
#include "inode.h"
#include "file.h"
#include "../common/shared_structs.h"

#define PORTFS_MAGIC 0x506F5254
#define MAX_STORAGE_PATH 256


static char storage_path[MAX_STORAGE_PATH];
static struct file* storage_filp = NULL;

static void portfs_put_super(struct super_block *sb) {
    struct portfs_superblock *msb = sb->s_fs_info;
    if (!msb)
        return;

    if (msb->filetable)
        vfree(msb->filetable);
    if (msb->block_bitmap)
        kfree(msb->block_bitmap);
    
    kfree(msb);
    sb->s_fs_info = NULL;
    kill_litter_super(sb);
}


static const struct super_operations portfs_super_ops = {
    .put_super = portfs_put_super,
};


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
    pr_info("portfs_init_superblock: Allocating memory for dsb and msb\n");

    struct portfs_disk_superblock *dsb =
        (struct portfs_disk_superblock *)kmalloc(sizeof(struct portfs_disk_superblock), GFP_KERNEL);
    if (!dsb)
    {
        pr_err("portfs_init_superblock: Could not allocate memory for "
               "struct portfs_disk_superblock");
        return ERR_PTR(-ENOMEM);
    }

    struct portfs_superblock *msb =
        (struct portfs_superblock *)kmalloc(sizeof(struct portfs_superblock), GFP_KERNEL);
    if (!msb)
    {
        pr_err("portfs_init_superblock: Could not allocate memory for "
               "struct portfs_superblock");
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

    msb->filetable = vmalloc(total_size);
    if (!(msb->filetable))
    {
        pr_err("portfs_init_filetable: Could not allocate memory, size is %zu\n", total_size);
        return -ENOMEM;
    }

    ssize_t bytes_read = 0;
    bytes_read = kernel_read(storage_filp, msb->filetable, total_size, &offset);
    if (bytes_read < 0)
    {
        pr_err("portfs_init_filetable: Failed to read filetable: %zd\n", bytes_read);
        kfree(msb->filetable);
        msb->filetable = NULL;
        return bytes_read;
    }
    if (bytes_read != total_size)
    {
        pr_err("portfs_init_filetable: Incomplete read of filetable: expected %zu, got %zd\n",
               total_size, bytes_read);
        kfree(msb->filetable);
        msb->filetable = NULL;
        return -EIO;
    }

    for (int i = 0; i < msb->max_file_count; ++i)
    {
        struct filetable_entry *entry = &msb->filetable[i];

        entry->sizeInBytes = be32_to_cpu(entry->sizeInBytes);
        entry->extentCount = be32_to_cpu(entry->extentCount);
        for (size_t i = 0; i < entry->extentCount; ++i)
        {
            entry->extents[i].startBlock = be32_to_cpu(entry->extents[i].startBlock);
            entry->extents[i].length = be32_to_cpu(entry->extents[i].length);
        }
    }
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


static int portfs_init_fs_data(struct super_block *sb, void *data)
{
    pr_info("portfs_init_fs_data: Initializing portfs service data\n");
    char *options = (char*)data;
    char *path = NULL;

    path = strstr(options, "path=");
    if (path)
    {
        path += 5;
        strncpy(storage_path, path, MAX_STORAGE_PATH - 1);
        storage_path[MAX_STORAGE_PATH - 1] = '\0';
    }

    pr_info("portfs_init_fs_data: Mounting filesystem with storage file: %s\n", storage_path);
    storage_filp = portfs_storage_init(storage_path);
    if (IS_ERR(storage_filp))
    {
        pr_err("portfs_init_fs_data: Error initializing storage\n");
        return PTR_ERR(storage_filp);
    }
    
    pr_info("portfs_init_fs_data: Initializing superblock\n");
    struct portfs_superblock *msb = portfs_init_superblock();
    if (IS_ERR(msb))
    {
        pr_err("portfs_init_fs_data: Error initializing superblock\n");
        return PTR_ERR(msb);
    }
    sb->s_fs_info = msb;

    pr_info("portfs_init_fs_data: Initializing filetable\n");
    int err = 0;
    err = portfs_init_filetable(msb);
    if (err)
    {
        pr_err("portfs_init_fs_data: Error initializing filetable\n");
        return err;
    }
    pr_info("portfs_init_fs_data: Initializing block bitmap\n");
    err = portfs_init_block_bitmap(msb);
    if (err)
    {
        pr_err("portfs_init_fs_data: Error initializing block bitmap\n");
        return err;
    }
    pr_info("portfs_init_fs_data: Finished\n");
    return 0;
}


static int portfs_fill_super(struct super_block *sb, void *data, int silent)
{
    pr_info("portfs_fill_super: Started\n");
    pr_info("portfs_fill_super: Calling portfs_init_fs_data()\n");
    int err = portfs_init_fs_data(sb, data);
    if (err)
    {
        pr_err("portfs_fill_super: Error initializing fs meta data\n");
        return err;
    }

    pr_info("portfs_fill_super: Creating root_inode\n");
    struct inode *root_inode;
    sb->s_magic = PORTFS_MAGIC;
    sb->s_op = &portfs_super_ops;

    root_inode = new_inode(sb);
    if (!root_inode)
    {
        pr_err("portfs_fill_super: Failed to create root_inode\n");
        filp_close(storage_filp, NULL);
        return -ENOMEM;
    }

    inode_init_owner(&nop_mnt_idmap, root_inode, NULL, S_IFDIR | 0755);
    root_inode->i_ino = 1;
    root_inode->i_mode = S_IFDIR | 0755;
    root_inode->i_sb = sb;
    root_inode->i_op = &portfs_dir_inode_operations;
    root_inode->i_fop = &portfs_dir_file_operations;

    sb->s_root = d_make_root(root_inode);
    if (!sb->s_root) {
        pr_err("portfs_fill_super: Failed to make_root\n");
        iput(root_inode);
        filp_close(storage_filp, NULL);
        return -ENOMEM;
    }
    pr_info("portfs_fill_super: Finished\n");
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
    if (storage_filp)
        filp_close(storage_filp, NULL);
    pr_info("portfs unloaded.\n");
}


module_init(portfs_init);
module_exit(portfs_exit);

MODULE_LICENSE("GPL");
