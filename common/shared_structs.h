#ifndef SHARED_STRUCTS_H
#define SHARED_STRUCTS_H

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <cstdint>
#endif // __KERNEL__

//const uint16_t MAX_EXTENTS = 8;
#define MAX_EXTENTS 8

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

struct disk_extent
{
    __be32 start_block;
    __be32 length;
}__attribute__((packed));

struct disk_filetable_entry
{
    char name[64];
    __be64 ino;
    __be64 sizeInBytes;
    __be16 extentCount;
    struct disk_extent extents[MAX_EXTENTS];
}__attribute__((packed));

#endif // __KERNEL__

#ifdef __cplusplus
extern "C"
{
#endif

struct extent
{
    uint32_t start_block;
    uint32_t length;
} __attribute__((packed));

struct filetable_entry
{
    char name[64];
    uint64_t ino;
    uint64_t sizeInBytes;
    uint16_t extentCount;
    struct extent extents[MAX_EXTENTS];
} __attribute__((packed));

struct portfs_superblock
{
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

#ifdef __cplusplus
} // extern "C"
#endif

#endif // SHARED_STRUCTS_H
