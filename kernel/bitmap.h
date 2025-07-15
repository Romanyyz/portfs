#ifndef BITMAP_H
#define BITMAP_H

#include <linux/types.h>

#include "shared_structs.h"

static inline int is_block_allocated(uint8_t *bitmap, uint32_t block)
{
    // TODO: probably need to lock bitmap
    return bitmap[block / 8] & (1 << (block % 8));
}


static inline void set_block_allocated(uint8_t *bitmap, uint32_t block)
{
    // TODO: probably need to lock bitmap
    bitmap[block / 8] |= (1 << (block % 8));
}


static inline void set_blocks_allocated(uint8_t *bitmap, uint32_t start_block, uint32_t length)
{
    // TODO: probably need to lock bitmap
    for (uint32_t i = start_block; i < start_block + length; ++i)
    {
        set_block_allocated(bitmap, i);
    }
}


static inline void clear_block_allocated(uint8_t *bitmap, uint32_t block)
{
    // TODO: probably need to lock bitmap
    bitmap[block / 8] &= ~(1 << (block % 8));
}


static inline int find_free_block(struct portfs_superblock *psb)
{
    // TODO: probably need to lock bitmap
    uint8_t *bitmap = psb->block_bitmap;
    uint32_t total_blocks = psb->total_blocks;
    for (uint32_t i = psb->data_start; i < total_blocks; ++i)
    {
        if (!is_block_allocated(bitmap, i))
        {
            return i;
        }
    }
    return -1;
}


static inline void clear_blocks_allocated(uint8_t *bitmap, uint32_t start_block, uint32_t length)
{
    // TODO: probably need to lock bitmap
    for (uint32_t i = start_block; i < start_block + length; ++i)
    {
        clear_block_allocated(bitmap, i);
    }
}

#endif // BITMAP_H
