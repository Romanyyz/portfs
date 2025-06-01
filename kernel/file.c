#include "file.h"

#include "portfs.h"
#include "bitmap.h"
#include "shared_structs.h"

#define BLOCK_ALLOC_SCALE 1000
#define BLOCK_ALLOC_MULTIPLIER 1500

static int allocate_extent(size_t bytes_to_allocate, struct portfs_superblock *psb, struct extent *extent)
{
    pr_info("allocate_extent: bytes_to_allocate = %zu", bytes_to_allocate);
    if (!extent || bytes_to_allocate < 0)
        return -EINVAL;

    size_t blocks_to_allocate = (bytes_to_allocate + psb->block_size - 1) / psb->block_size;
    const size_t new_blocks_to_allocate = (blocks_to_allocate * BLOCK_ALLOC_MULTIPLIER) / BLOCK_ALLOC_SCALE;

    // TODO: probably need to lock bitmap
    size_t curr_block_count = 0;
    uint32_t i = psb->data_start;
    const uint32_t total_blocks = psb->total_blocks;
    uint8_t *block_bitmap = psb->block_bitmap;
    for (; i < total_blocks; ++i)
    {
        if (!is_block_allocated(block_bitmap, i))
        {
            ++curr_block_count;
            if (curr_block_count == new_blocks_to_allocate)
            {
                break;
            }
        }
        else
        {
            curr_block_count = 0;
        }
    }

    if (curr_block_count != new_blocks_to_allocate)
        return -ENOMEM;

    size_t block_to_allocate = i - new_blocks_to_allocate;
    for (size_t j = 0; j < new_blocks_to_allocate; ++j)
    {
        set_block_allocated(block_bitmap, ++block_to_allocate);
    }

    extent->start_block = (i - new_blocks_to_allocate) + 1;
    extent->length = new_blocks_to_allocate;
    return 0;
}


static int portfs_iterate_shared(struct file *filp, struct dir_context *ctx)
{
    struct inode *inode = filp->f_inode;
    struct portfs_superblock *psb = inode->i_sb->s_fs_info;

    if (ctx->pos >= psb->max_file_count)
        return 0;

    if (!dir_emit_dots(filp, ctx))
        return -ENOMEM;

    const size_t max_files = psb->max_file_count;
    const size_t start_index = ctx->pos - 2;
    pr_info("portfs_iterate_shared: Starting from index %zu", start_index);
    for (size_t i = start_index; i < max_files; ++i)
    {
        struct filetable_entry *file_entry = &psb->filetable[i];
        if (file_entry->name[0] == '\0')
            continue;

        pr_info("portfs_iterate_shared: Found file: %s at index %zu", file_entry->name, i);
        if (!dir_emit(ctx, file_entry->name, strlen(file_entry->name), file_entry->ino, DT_REG))
            return -ENOMEM;

        ctx->pos = i + 1;
    }

    ctx->pos += 2;

    pr_info("portfs_iterate_shared: Finished iteration, setting ctx->pos to %lld", ctx->pos);
    return 0;
}


static int portfs_file_open(struct inode *inode, struct file *filp)
{
    pr_info("portfs_file_open: Opening file with flags 0x%x", filp->f_flags);

    int err = inode_permission(file_mnt_idmap(filp), inode, MAY_READ | MAY_WRITE);
    if (err)
    {
        pr_err("portfs_file_open: Error opening file: %s", filp->f_path.dentry->d_name.name);
        return err;
    }

    struct filetable_entry *file_entry = inode->i_private;
    if (filp->f_flags & O_TRUNC)
    {
        pr_info("portfs_file_open: truncating file to zero length.");
        file_entry->sizeInBytes = 0;
        inode->i_size = 0;
        mark_inode_dirty(inode);
    }

    pr_info("portfs_file_open: File successfuly opened: %s", filp->f_path.dentry->d_name.name);
    return 0;
}


static int portfs_release_file(struct inode *inode, struct file *filp)
{
    pr_info("portfs_release_file: Closing file.");
    pr_info("portfs_release_file: inode %lu, i_count=%d\n", inode->i_ino, atomic_read(&inode->i_count));
    return 0;
}


static ssize_t portfs_file_read(struct file *filp, char __user *buf, size_t count, loff_t *pos)
{
    pr_info("portfs_file_read: Read from file.");
    struct inode *inode = filp->f_inode;
    if (!inode || !buf)
    {
        return -EFAULT;
    }

    struct portfs_superblock *psb = inode->i_sb->s_fs_info;
    struct filetable_entry *file_entry = inode->i_private;

    if (*pos >= file_entry->sizeInBytes)
        return 0;
    if (*pos + count > file_entry->sizeInBytes)
        count = file_entry->sizeInBytes - *pos;

    loff_t global_file_offset = psb->data_start;
    size_t current_size_extent_bytes = 0;
    for (size_t i = 0; i < file_entry->extentCount; ++i)
    {
        const size_t start_extent_bytes = file_entry->extents[i].start_block * psb->block_size;
        const size_t end_extent_bytes = (file_entry->extents[i].start_block + file_entry->extents[i].length) * psb->block_size;
        const size_t next_size_extent_bytes = end_extent_bytes - start_extent_bytes;
        current_size_extent_bytes += next_size_extent_bytes;
        if (*pos < current_size_extent_bytes)
        {
            global_file_offset = start_extent_bytes + (*pos);
        }
    }

    char* kbuf = NULL;
    kbuf = kmalloc(count, GFP_KERNEL);
    if (!kbuf)
    {
        pr_err("portfs_file_read: Could not allocate memory");
        return -ENOMEM;
    }

    ssize_t bytes_read = kernel_read(storage_filp, kbuf, count, &global_file_offset);
    if (bytes_read > 0)
    {
        if (copy_to_user(buf, kbuf, bytes_read))
        {
            bytes_read = -EFAULT;
        }
        else
        {
            *pos += bytes_read;
        }
    }

    kfree(kbuf);
    return bytes_read;
}


static ssize_t portfs_file_write(struct file *filp, const char __user *buf, size_t count, loff_t *pos)
{
    pr_info("portfs_file_write: Write to file. Pos = %lld", *pos);
    pr_info("portfs_file_write: filp->f_flags = 0x%x\n", filp->f_flags);

    struct inode *inode = filp->f_inode;
    if (!inode || !buf)
    {
        return -EFAULT;
    }

    struct portfs_superblock *psb = inode->i_sb->s_fs_info;
    struct filetable_entry *file_entry = inode->i_private;

    if (filp->f_flags & O_APPEND)
    {
        *pos = file_entry->sizeInBytes;
    }

    if (file_entry->extentCount == 0)
    {
        pr_info("portfs_file_write: Allocating extent.");
        int err = allocate_extent(count, psb, &(file_entry->extents[0]));
        if (err)
        {
            pr_err("portfs_file_write: Could not allocate extent.");
            return err;
        }
        file_entry->extentCount = 1;
    }

    char* kbuf = NULL;
    kbuf = kmalloc(count, GFP_KERNEL);
    if (!kbuf)
    {
        pr_err("portfs_file_read: Could not allocate memory");
        return -ENOMEM;
    }

    if (copy_from_user(kbuf, buf, count))
    {
        kfree(kbuf);
        return -EFAULT;
    }

    loff_t global_file_offset = file_entry->extents[0].start_block * psb->block_size + *pos;
    pr_info("portfs_file_write: Write to file. global_file_offset = %lld", global_file_offset);
    ssize_t bytes_written = kernel_write(storage_filp, kbuf, count, &global_file_offset);

    if (bytes_written > 0)
    {
        *pos += bytes_written;
        if (*pos > file_entry->sizeInBytes)
        {
            file_entry->sizeInBytes = *pos;
            inode->i_size = *pos;
            mark_inode_dirty(inode);
        }
    }

    pr_info("portfs_file_write: Write to file finished. New Pos = %lld", *pos);
    kfree(kbuf);

    return bytes_written;
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
