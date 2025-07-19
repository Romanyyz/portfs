#include "extent_alloc.h"

#include "linux/fs.h"
#include "linux/slab.h"

#include "extent_tree.h"
#include "portfs.h"
#include "shared_structs.h"
#include "bitmap.h"

#define BLOCK_ALLOC_SCALE 1000
#define BLOCK_ALLOC_MULTIPLIER 1500

inline size_t portfs_max_extents(struct portfs_superblock *psb)
{
    return DIRECT_EXTENTS + (psb->block_size / sizeof(struct extent));
}

int portfs_allocate_memory(struct portfs_superblock *psb,
                           struct filetable_entry *file_entry,
                           size_t bytes_to_allocate)
{
    pr_info("portfs_allocate_memory: Allocating %zu bytes for file %s",
            bytes_to_allocate, file_entry->name);
    if (!psb || ! file_entry || bytes_to_allocate == 0)
        return -EINVAL;

    size_t blocks_to_allocate = (bytes_to_allocate + psb->block_size - 1) / psb->block_size;
    blocks_to_allocate = (blocks_to_allocate * BLOCK_ALLOC_MULTIPLIER) / BLOCK_ALLOC_SCALE;

    struct rb_root free_extent_tree = RB_ROOT;
    portfs_build_extent_tree(psb, &free_extent_tree);

    size_t free_ext_idx = file_entry->extent_count;
    if (free_ext_idx >= DIRECT_EXTENTS)
    {
        portfs_alloc_indirect_extents(psb, file_entry);
    }

    size_t max_extents = portfs_max_extents(psb);
    ssize_t remaining_blocks = blocks_to_allocate;
    struct rb_node *node;
    for (node = rb_first(&free_extent_tree); node; node = rb_next(node))
    {
        if (free_ext_idx >= max_extents)
        {
            pr_warn("portfs_allocate_memory: Too many extents for file %s", file_entry->name);
            break;
        }

        struct free_extent *free_ext = rb_entry(node, struct free_extent, node);
        remaining_blocks -= free_ext->length;

        pr_info("portfs_allocate_memory: using free extent [%u ... %u)\n",
                 free_ext->start_block,
                 free_ext->start_block + free_ext->length);


        set_blocks_allocated(psb->block_bitmap,
                             free_ext->start_block,
                             free_ext->length);

        if (free_ext_idx < DIRECT_EXTENTS)
        {
            file_entry->direct_extents[free_ext_idx].start_block = free_ext->start_block;
            file_entry->direct_extents[free_ext_idx].length = free_ext->length;
        }
        else
        {
            if (!file_entry->indirect_extents)
            {
                portfs_alloc_indirect_extents(psb, file_entry);
            }
            file_entry->indirect_extents[free_ext_idx - DIRECT_EXTENTS].start_block = free_ext->start_block;
            file_entry->indirect_extents[free_ext_idx - DIRECT_EXTENTS].length = free_ext->length;
        }
        ++free_ext_idx;

        if (remaining_blocks <= 0)
            break;
    }

    file_entry->extent_count = free_ext_idx;
    portfs_destroy_extent_tree(&free_extent_tree);
    if (remaining_blocks > 0)
        pr_err("portfs_allocate_memory: Not enough space. Remaining blocks: %zu", remaining_blocks);

    return (remaining_blocks <= 0) ? 0 : -ENOSPC;
}


int portfs_alloc_indirect_extents(struct portfs_superblock *psb,
                                  struct filetable_entry *file_entry)
{
    if (!file_entry->indirect_extents)
    {
        file_entry->indirect_extents = kzalloc(psb->block_size, GFP_KERNEL);
        if (!file_entry->indirect_extents)
            return -ENOMEM;

        if (file_entry->extents_block == 0)
        {
            int free_block = find_free_block(psb);
            if (free_block == -1)
            {
                pr_err("portfs_alloc_indirect_extents: Failed to find free block");
                return -ENOSPC;
            }
            set_block_allocated(psb->block_bitmap, free_block);
            file_entry->extents_block = free_block;
        }
        else
        {
            loff_t offset = file_entry->extents_block * psb->block_size;
            ssize_t bytes_read = kernel_read(storage_filp, file_entry->indirect_extents, psb->block_size, &offset);
            if (bytes_read != psb->block_size)
            return -EIO;
        }
    }
    return 0;
}


size_t portfs_get_allocated_size(const struct filetable_entry *entry,
                                 size_t block_size)
{
    size_t total_blocks = 0;
    for (size_t i = 0; i < entry->extent_count; ++i)
    {
        const struct extent *ext = get_extent(entry, i);
        total_blocks += ext->length;
    }

    return total_blocks * block_size;
}
