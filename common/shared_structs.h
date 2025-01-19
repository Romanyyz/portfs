#ifndef SHARED_STRUCTS_H
#define SHARED_STRUCTS_H

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <cstdint>
#endif // __KERNEL__

#ifdef __KERNEL__

struct portfs_disk_superblock {
    __be32 magic_number;
    __be32 block_size;
    __be32 total_blocks;
    __be32 filetable_start;    // Offset in blocks
    __be32 filetable_size;     // Size in blocks
    __be32 block_bitmap_start; // Offset in blocks
    __be32 block_bitmap_size;  // Size in blocks
    __be32 data_start;         // Offset in blocks
    __be32 max_file_count;
    __be32 checksum;

    __be64 last_mount_time;
    __be64 last_write_time;
    __be32 flags;
} __attribute__((packed));

struct disk_filetable_entry
{
    char name[64];
    __be64 startBlock;
    __be64 sizeInBlocks;
    __be64 sizeInBytes;
};
#endif // __KERNEL__

extern "C"
{

struct filetable_entry
{
    char name[64];
    uint64_t startBlock;
    uint64_t sizeInBlocks;
    uint64_t sizeInBytes;
};

struct portfs_superblock {
    uint32_t magic_number;
    uint32_t block_size;
    uint32_t total_blocks;
    uint32_t filetable_start;    // Offset in blocks
    uint32_t filetable_size;     // Size in blocks
    uint32_t block_bitmap_start; // Offset in blocks
    uint32_t block_bitmap_size;  // Size in blocks
    uint32_t data_start;         // Offset in blocks
    uint32_t max_file_count;
    uint32_t checksum;

    uint64_t last_mount_time;
    uint64_t last_write_time;
    uint32_t flags;

#ifdef __KERNEL__
    struct filetable_entry *filetable;
    uint8_t *block_bitmap;

    struct super_block *super;
#endif
} __attribute__((packed));

} // extern "C"

#endif // SHARED_STRUCTS_H
