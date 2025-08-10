#include "directory.h"

#include <linux/fs.h>
#include <linux/string.h>
#include "linux/slab.h"

#include "portfs.h"
#include "inode.h"
#include "file.h"
#include "block_bitmap.h"

static int portfs_de_alloc_block(struct portfs_superblock *psb, struct filetable_entry *parent_dir)
{
    uint32_t dir_block = find_free_block(psb);
    if (dir_block == -1)
        return -ENOSPC;
    set_block_allocated(psb->block_bitmap, dir_block);
    set_dir_block(parent_dir, dir_block);
    return dir_block;
}


int portfs_load_dir_data(struct portfs_superblock *psb,
                         struct filetable_entry *parent_dir)
{
    uint16_t num_entries = get_max_dir_entries(psb);
    uint32_t mem_size = num_entries * sizeof(*parent_dir->dir_entries);

    parent_dir->dir_entries = kzalloc(mem_size, GFP_KERNEL);
    if (!parent_dir->dir_entries)
        return -ENOMEM;

    uint32_t dir_block = get_dir_block(parent_dir);
    if (dir_block == 0)
    {
        dir_block = portfs_de_alloc_block(psb, parent_dir);
        if (dir_block < 0)
        {
            kfree(parent_dir->dir_entries);
            parent_dir->dir_entries = NULL;
            return dir_block;
        }
    }
    else
    {
        uint32_t disk_size = psb->block_size;
        struct disk_dir_entry *buf = kmalloc(disk_size, GFP_KERNEL);
        if (!buf)
            return -ENOMEM;

        loff_t offset = dir_block * psb->block_size;
        ssize_t bytes_read = kernel_read(storage_filp, buf, disk_size, &offset);
        if (bytes_read < 0)
        {
            kfree(parent_dir->dir_entries);
            kfree(buf);
            return bytes_read;
        }
        if (bytes_read != disk_size)
        {
            kfree(parent_dir->dir_entries);
            kfree(buf);
            return -EIO;
        }

        for (int i = 0; i < num_entries; ++i)
        {
            struct disk_dir_entry *disk_entry = &buf[i];
            struct dir_entry *entry = &parent_dir->dir_entries[i];

            entry->inode_number = be32_to_cpu(disk_entry->inode_number);
            strncpy(entry->name, disk_entry->name, sizeof(entry->name));
            entry->name[sizeof(entry->name) - 1] = '\0';
        }
        kfree(buf);
    }
    return 0;
}


bool portfs_is_dir_empty(struct portfs_superblock *psb, struct filetable_entry *dir)
{
    if (get_dir_block(dir) == 0)
        return true;

    if (!dir->dir_entries)
        portfs_load_dir_data(psb, dir);

    uint16_t num_entries = get_max_dir_entries(psb);
    for (int i = 0; i < num_entries; ++i)
    {
        struct dir_entry *entry = &dir->dir_entries[i];
        if (entry->inode_number != 0)
            return false;
    }

    return true;
}


int portfs_de_add(struct portfs_superblock *psb,
                  struct filetable_entry *parent_dir,
                  struct dir_entry *dir_entry)
{
    if (!parent_dir || !dir_entry)
        return -EINVAL;
    if (!is_directory(parent_dir))
        return -ENOTDIR;

    if (!parent_dir->dir_entries)
    {
        portfs_load_dir_data(psb, parent_dir);
    }

    uint16_t num_entries = get_max_dir_entries(psb);
    for (int i = 0; i < num_entries; ++i)
    {
        struct dir_entry *entry = &parent_dir->dir_entries[i];
        if (entry->inode_number == 0)
        {
            entry->inode_number = dir_entry->inode_number;
            strncpy(entry->name, dir_entry->name, sizeof(entry->name));
            break;
        }
    }

    return 0;
}

struct dir_entry *portfs_de_find(struct portfs_superblock *psb,
                                 struct filetable_entry *parent_dir,
                                 const char *name)
{
    if (!parent_dir || !name)
        return ERR_PTR(-EINVAL);
    if (!is_directory(parent_dir))
        return ERR_PTR(-ENOTDIR);

    if (!parent_dir->dir_entries)
        portfs_load_dir_data(psb, parent_dir);

    uint16_t num_entries = get_max_dir_entries(psb);
    for (int i = 0; i < num_entries; ++i)
    {
        struct dir_entry *curr_entry = &parent_dir->dir_entries[i];
        if (strcmp(curr_entry->name, name) == 0)
        {
            return curr_entry;
        }
    }

    return NULL;
}


struct dir_entry *portfs_de_remove(struct portfs_superblock *psb,
                                   struct filetable_entry *parent_dir,
                                   const char *name)
{
    if (!psb || !parent_dir || !name)
        return ERR_PTR(-EINVAL);
    if (!is_directory(parent_dir))
        return ERR_PTR(-ENOTDIR);

    pr_info("Removing directory entry '%s' from parent directory\n", name);

    if (!parent_dir->dir_entries)
        portfs_load_dir_data(psb, parent_dir);

    uint16_t num_entries = get_max_dir_entries(psb);
    for (int i = 0; i < num_entries; ++i)
    {
        struct dir_entry *curr_entry = &parent_dir->dir_entries[i];
        if (strcmp(curr_entry->name, name) == 0)
        {
            curr_entry->inode_number = 0;
            memset(curr_entry->name, 0, MAX_NAME_LENGTH);
        }
    }

    return NULL;
}
