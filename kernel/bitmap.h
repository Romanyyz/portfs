#ifndef BITMAP_H
#define BITMAP_H

#include <linux/types.h>

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
    for (uint32_t i = start_block; i <= length; ++i)
    {
        set_block_allocated(bitmap, i);
    }
}


static inline void clear_block_allocated(uint8_t *bitmap, uint32_t block)
{
    // TODO: probably need to lock bitmap
    bitmap[block / 8] &= ~(1 << (block % 8));
}


static inline int find_free_block(uint8_t *bitmap, uint32_t total_blocks)
{
    // TODO: probably need to lock bitmap
    for (uint32_t i = 0; i < total_blocks; ++i)
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
    for (uint32_t i = start_block; i <= length; ++i)
    {
        clear_block_allocated(bitmap, i);
    }
}

#endif // BITMAP_H
