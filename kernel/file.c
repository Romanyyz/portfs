#include "file.h"

#include "portfs.h"
#include "../common/shared_structs.h"

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
