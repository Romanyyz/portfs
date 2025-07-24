#include "file.h"

#include "linux/err.h"
#include "linux/fs.h"
#include "linux/fs_types.h"
#include "linux/stat.h"
#include "linux/types.h"

#include "portfs.h"
#include "bitmap.h"
#include "extent_tree.h"
#include "extent_alloc.h"
#include "shared_structs.h"
#include "directory.h"
#include "inode.h"

static inline uint8_t mode_to_dtype(uint16_t mode)
{
    if (S_ISDIR(mode))
        return DT_DIR;
    else if (S_ISREG(mode))
        return DT_REG;
    else
        return DT_UNKNOWN;
}


static int portfs_iterate_shared(struct file *filp, struct dir_context *ctx)
{
    pr_info("portfs_iterate_shared: Begin");
    struct inode *inode = filp->f_inode;
    struct portfs_superblock *psb = inode->i_sb->s_fs_info;

    if (ctx->pos >= psb->max_file_count)
        return 0;

    if (ctx->pos == 0)
    {
        if (!dir_emit_dots(filp, ctx))
            return -EFAULT;
    }

    struct filetable_entry *dir = inode->i_private;
    if (!dir)
    {
        pr_err("portfs_iterate_shared: Directory entry not found");
        return -ENOENT;
    }

    if (!dir->dir_entries)
        portfs_load_dir_data(psb, dir);

    uint16_t num_entries = get_max_dir_entries(psb);

    pr_info("portfs_iterate_shared: Starting from index %lld", ctx->pos - 2);
    for (; ctx->pos - 2 < num_entries; ctx->pos++)
    {
        struct dir_entry *d_entry = &dir->dir_entries[ctx->pos - 2];
        if (d_entry->inode_number == 0)
            continue;

        pr_info("portfs_iterate_shared: Found file: %s at index %lld", d_entry->name, ctx->pos - 2);
        struct inode *child_inode = portfs_get_inode_by_number(inode->i_sb, d_entry->inode_number);
        if (!child_inode || IS_ERR(child_inode))
            continue;

        pr_info("portfs_iterate_shared: Emitting file: %s at index %lld", d_entry->name, ctx->pos - 2);
        if (!dir_emit(ctx, d_entry->name, strlen(d_entry->name),
                      d_entry->inode_number, mode_to_dtype(child_inode->i_mode)))
        {
            iput(child_inode);
            return -ENOMEM;
        }
        iput(child_inode);
    }

    pr_info("portfs_iterate_shared: Finished iteration, setting ctx->pos to %lld", ctx->pos);
    return 0;
}


static int portfs_file_open(struct inode *inode, struct file *filp)
{
    pr_info("portfs_file_open: Opening file with flags 0x%x", filp->f_flags);

    int err = inode_permission(file_mnt_idmap(filp), inode, MAY_READ | MAY_WRITE);
    if (err)
    {
        pr_err("portfs_file_open: Error opening file, invalid permissions: %s", filp->f_path.dentry->d_name.name);
        return err;
    }

    pr_info("portfs_file_open: File successfuly opened: %s", filp->f_path.dentry->d_name.name);
    return 0;
}


static int portfs_release_file(struct inode *inode, struct file *filp)
{
    pr_info("portfs_release_file: Closing file.");
    pr_info("portfs_release_file: inode %lu, i_count=%d\n", inode->i_ino, atomic_read(&inode->i_count));
    struct filetable_entry *file_entry = inode->i_private;
    if (file_entry && file_entry->indirect_extents)
    {
        kfree(file_entry->indirect_extents);
        file_entry->indirect_extents = NULL;
    }

    return 0;
}


static loff_t portfs_calc_global_offset(const struct portfs_superblock *psb,
                                        const struct filetable_entry *entry,
                                        loff_t local_offset)
{
    pr_info("portfs_calc_global_offset: Local offset = %lld", local_offset);

    loff_t offset_remaining = local_offset;

    for (size_t i = 0; i < entry->file.extent_count; ++i)
    {
        const struct extent *ext = get_extent(entry, i);
        loff_t ext_size = ext->length * psb->block_size;

        if (offset_remaining < ext_size)
        {
            pr_info("portfs_calc_global_offset: Using block %u", ext->start_block);
            loff_t ret = ext->start_block * psb->block_size + offset_remaining;
            pr_info("portfs_calc_global_offset: Calculated global offset = %lld", ret);
            return ret;
        }

        offset_remaining -= ext_size;
    }

    return -EFAULT;
}


static ssize_t portfs_calc_available_bytes(const struct portfs_superblock *psb,
                                           const struct filetable_entry *entry,
                                           loff_t local_offset)
{
    size_t global_offset = 0;

    for (size_t i = 0; i < entry->file.extent_count; ++i)
    {
        const struct extent *ext = get_extent(entry, i);
        global_offset += ext->length * psb->block_size;
        if (global_offset > local_offset)
        {
            size_t remaining = global_offset - local_offset;
            return remaining;
        }
    }

    return -EFAULT;
}


static ssize_t portfs_file_read(struct file *filp, char __user *buf, size_t count, loff_t *pos)
{
    pr_info("portfs_file_read: Read from file.");
    struct inode *inode = filp->f_inode;
    if (!inode || !buf)
    {
        return -EFAULT;
    }

    struct filetable_entry *file_entry = inode->i_private;
    if (*pos >= file_entry->size_in_bytes)
        return 0;
    if (*pos + count > file_entry->size_in_bytes)
        count = file_entry->size_in_bytes - *pos;


    struct portfs_superblock *psb = inode->i_sb->s_fs_info;
    if (file_entry->file.extent_count >= DIRECT_EXTENTS
        && !file_entry->indirect_extents)
    {
        portfs_alloc_indirect_extents(psb, file_entry);
    }

    char* kbuf = NULL;
    kbuf = kmalloc(count, GFP_KERNEL);
    if (!kbuf)
    {
        pr_err("portfs_file_read: Could not allocate memory");
        return -ENOMEM;
    }

    ssize_t bytes_read_total = 0;
    size_t requested = count;
    while (requested > 0)
    {
        loff_t global_offset = portfs_calc_global_offset(psb, file_entry, *pos);
        size_t bytes_can_read = portfs_calc_available_bytes(psb, file_entry, *pos);
        size_t bytes_to_read = min(requested, bytes_can_read);
        if (bytes_to_read == 0)
        {
            pr_warn("portfs_file_read: No more bytes to read or available. Breaking loop.");
            break;
        }

        ssize_t bytes_read = kernel_read(storage_filp, kbuf + bytes_read_total, bytes_to_read, &global_offset);
        if (bytes_read != bytes_to_read)
        {
            kfree(kbuf);
            pr_err("portfs_file_read: Error reading from storage file");
            return -EIO;
        }

        *pos += bytes_read;
        requested -= bytes_read;
        bytes_read_total += bytes_read;
    }

    if (bytes_read_total == count)
    {
        if (copy_to_user(buf, kbuf, bytes_read_total))
        {
            bytes_read_total = -EFAULT;
        }
    }

    kfree(kbuf);
    return bytes_read_total;
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
        *pos = file_entry->size_in_bytes;
    }

    const size_t available_size = portfs_get_allocated_size(file_entry, psb->block_size)
                                    - file_entry->size_in_bytes;
    if (available_size < count)
    {
        int err = portfs_allocate_memory(psb, file_entry, count - available_size);
        if (err)
        {
            pr_err("portfs_file_write: Could not allocate memory");
            return err;
        }
    }

    char* kbuf = kmalloc(count, GFP_KERNEL);
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

    size_t bytes_written_total = 0;
    while (count > 0)
    {
        loff_t global_offset = portfs_calc_global_offset(psb, file_entry, *pos);
        if (global_offset < psb->data_start * psb->block_size)
        {
            pr_err("portfs_file_write: Invalid offset, offset = %llu", global_offset);
            break;
        }
        size_t available_bytes = portfs_calc_available_bytes(psb, file_entry, *pos);
        if (available_bytes <= 0)
        {
            pr_warn("portfs_file_write: No available bytes");
            break;
        }
        size_t bytes_to_write = min(available_bytes, count);

        pr_info("portfs_file_write: Write to file. global_offset = %lld, bytes = %ld",
                global_offset, bytes_to_write);

        ssize_t bytes_written = kernel_write(storage_filp,
                                             kbuf + bytes_written_total,
                                             bytes_to_write,
                                             &global_offset);
        if (bytes_written != bytes_to_write)
        {
            break;
        }

        count -= bytes_written;
        *pos += bytes_written;
        bytes_written_total += bytes_written;
    }

    if (count == 0)
    {
        if (*pos > file_entry->size_in_bytes)
        {
            file_entry->size_in_bytes = *pos;
            inode->i_size = *pos;
            mark_inode_dirty(inode);
        }
    }
    else
    {
        pr_err("portfs_file_write: Failed to write all bytes");
        kfree(kbuf);
        return -EIO;
    }

    pr_info("portfs_file_write: Write to file finished. New Pos = %lld", *pos);
    kfree(kbuf);

    return bytes_written_total;
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
