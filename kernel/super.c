#include "portfs.h"
#include "inode.h"
#include "file.h"
#include "shared_structs.h"

#define PORTFS_MAGIC 0x506F5254
#define MAX_STORAGE_PATH 256
#define WRITE_BUFFER_SIZE 1*1024*1024

static char storage_path[MAX_STORAGE_PATH];
struct file* storage_filp;

static void portfs_put_super(struct super_block *sb)
{
    pr_info("Killing superblock\n");

    struct portfs_superblock *msb = sb->s_fs_info;
    if (!msb)
        return;

    if (msb->filetable)
        vfree(msb->filetable);
    if (msb->block_bitmap)
        kfree(msb->block_bitmap);

    kfree(msb);
    sb->s_fs_info = NULL;
    generic_shutdown_super(sb);

    if (storage_filp)
        filp_close(storage_filp, NULL);
}


static void portfs_evict_inode(struct inode *inode)
{
    pr_info("portfs_evict_inode: inode %lu\n", inode->i_ino);
    pr_info("portfs_evict_inode: inode %lu, i_count=%d\n", inode->i_ino, atomic_read(&inode->i_count));
    inode->i_private = NULL;
    clear_inode(inode);
}


static int portfs_sync_superblock(struct super_block *sb)
{
    pr_info("portfs_sync_superblock: Syncing superblock");
    if (!sb)
    {
        pr_err("portfs_write_block_bitmap: Invalid parameter");
        return -EINVAL;
    }

    struct portfs_disk_superblock dsb;
    struct portfs_superblock *msb = sb->s_fs_info;

    memset(&dsb, 0, sizeof(dsb));
    dsb.magic_number = cpu_to_be32(msb->magic_number);
    dsb.block_size = cpu_to_be32(msb->block_size);
    dsb.total_blocks = cpu_to_be32(msb->total_blocks);
    dsb.block_bitmap_start = cpu_to_be32(msb->block_bitmap_start);
    dsb.block_bitmap_size = cpu_to_be32(msb->block_bitmap_size);
    dsb.filetable_start = cpu_to_be32(msb->filetable_start);
    dsb.filetable_size = cpu_to_be32(msb->filetable_size);
    dsb.data_start = cpu_to_be32(msb->data_start);
    dsb.checksum = cpu_to_be32(msb->checksum);
    dsb.max_file_count = cpu_to_be32(msb->max_file_count);

    ssize_t bytes_written = kernel_write(storage_filp, &dsb, sizeof(dsb), 0);
    if (bytes_written < 0)
    {
        pr_err("portfs_sync_superblock: Failed to write superblock");
        return bytes_written;
    }

    return 0;
}

static int portfs_write_filetable(struct portfs_superblock *psb)
{
    pr_info("portfs_write_filetable: Writing filetable");
    if (!psb)
    {
        pr_err("portfs_write_block_bitmap: Invalid parameter");
        return -EINVAL;
    }

    struct disk_filetable_entry *disk_entries_buf;
    struct filetable_entry *filetable = psb->filetable;

    const size_t buf_entries = WRITE_BUFFER_SIZE / sizeof(*disk_entries_buf);
    const size_t buf_size = buf_entries * sizeof(*disk_entries_buf);
    disk_entries_buf = kmalloc(buf_size, GFP_KERNEL);
    if (!disk_entries_buf)
    {
        pr_err("portfs_write_filetable: Failed to allocate memory");
        return -ENOMEM;
    }

    loff_t current_offset = psb->filetable_start * psb->block_size;
    for (int i = 0; i < psb->max_file_count; )
    {
        int curr_entries = 0;
        for (; curr_entries < buf_entries && i < psb->max_file_count; ++curr_entries, ++i)
        {
            struct filetable_entry *src_entry = &filetable[i];
            struct disk_filetable_entry *dst_entry = &disk_entries_buf[curr_entries];

            memcpy(dst_entry->name, src_entry->name, sizeof(dst_entry->name));
            dst_entry->ino = cpu_to_be64(src_entry->ino);
            dst_entry->sizeInBytes = cpu_to_be64(src_entry->sizeInBytes);
            dst_entry->extentCount = cpu_to_be16(src_entry->extentCount);

            for (int k = 0; k < src_entry->extentCount; ++k)
            {
                dst_entry->extents[k].start_block = cpu_to_be32(src_entry->extents[k].start_block);
                dst_entry->extents[k].length = cpu_to_be32(src_entry->extents[k].length);
            }
        }
        size_t curr_size = curr_entries * sizeof(*disk_entries_buf);
        ssize_t bytes_written = kernel_write(storage_filp, (void *)disk_entries_buf, curr_size, &current_offset);
        if (bytes_written != curr_size)
        {
            pr_err("portfs_write_filetable: Failed to write filetable");
            kfree(disk_entries_buf);
            return -EIO;
        }
        memset(disk_entries_buf, 0, curr_size);
    }

    kfree(disk_entries_buf);
    return 0;
}

static int portfs_write_block_bitmap(struct portfs_superblock *psb)
{
    pr_info("portfs_write_block_bitmap: Writing block bitmap");
    if (!psb)
    {
        pr_err("portfs_write_block_bitmap: Invalid parameter");
        return -EINVAL;
    }

    uint8_t *block_bitmap = psb->block_bitmap;
    loff_t file_offset = psb->block_bitmap_start * psb->block_size;
    loff_t bitmap_offset = 0;
    size_t remaining_bytes = psb->block_bitmap_size * psb->block_size;
    int ret = 0;
    while (remaining_bytes > 0)
    {
        size_t bytes_to_write = min(WRITE_BUFFER_SIZE, remaining_bytes);
        ssize_t bytes_written = kernel_write(storage_filp,
                                            block_bitmap + bitmap_offset,
                                            bytes_to_write,
                                            &file_offset);
        if (bytes_written < 0)
        {
            pr_err("portfs_write_block_bitmap: kernel_write failed");
            ret = bytes_written;
            break;
        }
        if ((size_t)bytes_written < bytes_to_write)
        {
            pr_err("portfs_write_block_bitmap: kernel_write short write");
            ret = -EIO;
            break;
        }
        bitmap_offset += bytes_to_write;
        remaining_bytes -= bytes_written;
    }

    if (ret == 0)
        pr_info("portfs_write_block_bitmap: Block bitmap written successfully");
    else
        pr_err("portfs_write_block_bitmap: Failed to write block bitmap");

    return ret;
}

static int portfs_sync_fs(struct super_block *sb, int wait)
{
    pr_info("portfs_sync_fs: Syncing portfs filesystem");
    int err = 0;
    err = portfs_sync_superblock(sb);
    if (err != 0)
    {
        pr_err("portfs_sync_fs: Failed to sync superblock");
        return err;
    }

    struct portfs_superblock *psb = sb->s_fs_info;
    err = portfs_write_filetable(psb);
    if (err != 0)
    {
        pr_err("portfs_sync_fs: Failed to write filetable");
        return err;
    }

    err = portfs_write_block_bitmap(psb);
    if (err != 0)
    {
        pr_err("portfs_sync_fs: Failed to write block bitmap");
        return err;
    }

    if (storage_filp)
    {
        err = vfs_fsync(storage_filp, 0);
        if (err != 0)
        {
            pr_err("portfs_sync_fs: Failed to sync storage file");
            return err;
        }
    }

    return 0;
}


static const struct super_operations portfs_super_ops = {
    .put_super = portfs_put_super,
    .evict_inode = portfs_evict_inode,
    .sync_fs    = portfs_sync_fs,
};


static int portfs_fill_superblock(struct portfs_superblock *msb, const struct portfs_disk_superblock *dsb)
{
    if (!msb || !dsb)
    {
        pr_err("portfs_fill_superblock: One of arguments was NULL");
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
    msb->super = NULL;
    return 0;
}


static struct portfs_superblock *portfs_init_superblock(void)
{
    pr_info("portfs_init_superblock: Allocating memory for dsb and msb\n");

    struct portfs_disk_superblock *dsb;
    dsb = kmalloc(sizeof(*dsb), GFP_KERNEL);
    if (!dsb)
    {
        pr_err("portfs_init_superblock: Could not allocate memory for "
               "struct portfs_disk_superblock");
        return ERR_PTR(-ENOMEM);
    }

    loff_t pos = 0;
    ssize_t bytes_read = kernel_read(storage_filp,
                                     dsb,
                                     sizeof(*dsb),
                                     &pos);
    if (bytes_read < 0)
    {
        pr_err("portfs_init_superblock: Error reading storage file");
        kfree(dsb);
        return ERR_PTR(bytes_read);
    }

    struct portfs_superblock *msb;
    msb = kmalloc(sizeof(*msb), GFP_KERNEL);
    if (!msb)
    {
        pr_err("portfs_init_superblock: Could not allocate memory for "
               "struct portfs_superblock");
        return ERR_PTR(-ENOMEM);
    }

    int err = portfs_fill_superblock(msb, dsb);
    if (err)
    {
        pr_err("portfs_init_superblock: Could not fill superblock");
        kfree(dsb);
        kfree(msb);
        return ERR_PTR(err);
    }

    kfree(dsb);
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

        entry->sizeInBytes = be64_to_cpu(entry->sizeInBytes);
        entry->extentCount = be16_to_cpu(entry->extentCount);
        entry->ino         = be64_to_cpu(entry->ino);
        for (size_t i = 0; i < entry->extentCount; ++i)
        {
            entry->extents[i].start_block = be32_to_cpu(entry->extents[i].start_block);
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
    msb->super = sb;

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

    insert_inode_hash(root_inode);

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
    pr_info("portfs unloaded.\n");
}


module_init(portfs_init);
module_exit(portfs_exit);

MODULE_LICENSE("GPL");
