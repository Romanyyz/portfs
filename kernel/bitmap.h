#ifndef BITMAP_H
#define BITMAP_H

#include <linux/types.h>

#include "shared_structs.h"

static inline int portfs_bitmap_is_set(uint8_t *bitmap, uint32_t bit)
{
    // TODO: probably need to lock bitmap
    return bitmap[bit / 8] & (1 << (bit % 8));
}


static inline void portfs_bitmap_set_bit(uint8_t *bitmap, uint32_t bit)
{
    // TODO: probably need to lock bitmap
    bitmap[bit / 8] |= (1 << (bit % 8));
}


static inline void portfs_bitmap_set_bits(uint8_t *bitmap, uint32_t start_bit, uint32_t length)
{
    // TODO: probably need to lock bitmap
    for (uint32_t i = start_bit; i < start_bit + length; ++i)
    {
        portfs_bitmap_set_bit(bitmap, i);
    }
}


static inline void portfs_bitmap_clear_bit(uint8_t *bitmap, uint32_t bit)
{
    // TODO: probably need to lock bitmap
    bitmap[bit / 8] &= ~(1 << (bit % 8));
}


static inline void portfs_bitmap_clear_bits(uint8_t *bitmap, uint32_t start_bit, uint32_t length)
{
    // TODO: probably need to lock bitmap
    for (uint32_t i = start_bit; i < start_bit + length; ++i)
    {
        portfs_bitmap_clear_bit(bitmap, i);
    }
}

#endif // BITMAP_H
