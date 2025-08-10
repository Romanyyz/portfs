#ifndef BLOCK_BITMAP_H
#define BLOCK_BITMAP_H

#include <linux/types.h>

#include "shared_structs.h"
#include "bitmap.h"

static inline int is_block_allocated(uint8_t *bitmap, uint32_t block)
{
    return portfs_bitmap_is_set(bitmap, block);
}


static inline void set_block_allocated(uint8_t *bitmap, uint32_t block)
{
    portfs_bitmap_set_bit(bitmap, block);
}


static inline void set_blocks_allocated(uint8_t *bitmap, uint32_t start_block, uint32_t length)
{
    portfs_bitmap_set_bits(bitmap, start_block, length);
}


static inline void clear_block_allocated(uint8_t *bitmap, uint32_t block)
{
    portfs_bitmap_clear_bit(bitmap, block);
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
    portfs_bitmap_clear_bits(bitmap, start_block, length);
}

#endif // BLOCK_BITMAP_H
