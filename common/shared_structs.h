/*
 * This header defines structures shared between kernel module (portfs) and
 * userspace utility.
 * Structures prefixed with 'portfs_disk_' represent on-disk layout.
 * Structures without 'disk' are in-memory and may differ.
 */

#ifndef SHARED_STRUCTS_H
#define SHARED_STRUCTS_H

#ifdef __KERNEL__
#include <linux/types.h>
typedef __be16 portfs_be16;
typedef __be32 portfs_be32;
typedef __be64 portfs_be64;
#else
#include <cstdint>
typedef uint16_t portfs_be16;
typedef uint32_t portfs_be32;
typedef uint64_t portfs_be64;
#endif // __KERNEL__

#define DIRECT_EXTENTS 4

struct portfs_disk_superblock {
    portfs_be32 magic_number;
    portfs_be32 block_size;
    portfs_be32 total_blocks;
    portfs_be32 filetable_start;    // Offset in blocks
    portfs_be32 filetable_size;     // Size in blocks
    portfs_be32 block_bitmap_start; // Offset in blocks
    portfs_be32 block_bitmap_size;  // Size in blocks
    //portfs_be32 ino_bitmap_start;   // Offset in blocks
    //portfs_be32 ino_bitmap_size;    // Size in blocks
    //portfs_be32 next_free_ino_hint; // last found free ino
    portfs_be32 data_start;         // Offset in blocks
    portfs_be32 max_file_count;
    portfs_be32 checksum;

    portfs_be64 last_mount_time;
    portfs_be64 last_write_time;
    portfs_be32 flags;
} __attribute__((packed));

struct disk_extent
{
    portfs_be32 start_block;
    portfs_be32 length;
}__attribute__((packed));

struct disk_file_data
{
    portfs_be16 extent_count;
    portfs_be32 extents_block;
    struct disk_extent direct_extents[DIRECT_EXTENTS];
} __attribute__((packed));

struct disk_dir_data
{
    portfs_be32 dir_block;
    portfs_be32 parent_dir_ino;
} __attribute__((packed));

struct disk_filetable_entry
{
    portfs_be32 ino;
    portfs_be16 mode;
    portfs_be64 size_in_bytes;
    union
    {
        struct disk_file_data file;
        struct disk_dir_data dir;
    };
} __attribute__((packed));

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

struct extent
{
    uint32_t start_block;
    uint32_t length;
};

struct file_data
{
    uint16_t extent_count;
    uint32_t extents_block;
    struct extent direct_extents[DIRECT_EXTENTS];
};

struct dir_data
{
    uint32_t dir_block;
    uint32_t parent_dir_ino;
};

struct dir_entry;
struct filetable_entry
{
    uint32_t ino;
    uint16_t mode;
    uint64_t size_in_bytes;
    union
    {
        struct file_data file;
        struct dir_data dir;
    };

#ifdef __KERNEL__
    struct extent *indirect_extents;
    struct dir_entry *dir_entries;
#endif // __KERNEL__
};

struct portfs_superblock
{
    uint32_t magic_number;
    uint32_t block_size;
    uint32_t total_blocks;
    uint32_t filetable_start;    // Offset in blocks
    uint32_t filetable_size;     // Size in blocks
    uint32_t block_bitmap_start; // Offset in blocks
    uint32_t block_bitmap_size;  // Size in blocks
    //uint32_t ino_bitmap_start;   // Offset in blocks
    //uint32_t ino_bitmap_size;    // Size in blocks
    //uint32_t next_free_ino_hint; // last found free ino
    uint32_t data_start;         // Offset in blocks
    uint32_t max_file_count;
    uint32_t checksum;

    uint64_t last_mount_time;
    uint64_t last_write_time;
    uint32_t flags;

#ifdef __KERNEL__
    struct filetable_entry *filetable;
    uint8_t *block_bitmap;
    uint8_t *ino_bitmap;

    struct super_block *super;
#endif
};

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus

#endif // SHARED_STRUCTS_H
