#ifndef PORTFS_H
#define PORTFS_H

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/types.h>
#include <linux/namei.h>
#include <linux/user_namespace.h>


// Function prototypes
static int portfs_create(struct mnt_idmap *idmap, struct inode *dir,
                           struct dentry *dentry, umode_t mode, bool excl);
static int portfs_unlink(struct inode *dir, struct dentry *dentry);

static int portfs_iterate_shared(struct file *filp, struct dir_context *ctx);

static int portfs_fill_super(struct super_block *sb, void *data, int silent);
static struct dentry *portfs_mount(struct file_system_type *fs_type,
                                   int flags, const char *dev_name, void *data);
static int portfs_create(struct mnt_idmap *idmap, struct inode *dir,
                         struct dentry *dentry, umode_t mode, bool excl);
static int portfsfs_unlink(struct inode *dir, struct dentry *dentry);
static int portfs_iterate_shared(struct file *filp, struct dir_context *ctx);
struct file* storage_init(char *path);

// Definition of structures
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .iterate_shared = portfs_iterate_shared, // open via ls
};

static struct inode_operations inode_ops = {
    .create = portfs_create,
    .unlink = portfs_unlink,
};

static struct file_system_type portfs_type = {
    .owner = THIS_MODULE,
    .name = "portfs",
    .mount = portfs_mount,
    .kill_sb = kill_litter_super,
    .fs_flags = FS_USERNS_MOUNT,
};


struct filetable_entry
{
    char name[128];
    uint32_t startBlock;
    uint32_t sizeInBlocks;
    uint32_t sizeInBytes;
};

struct portfs_superblock {
    uint32_t magic_number;
    uint32_t block_size;
    uint32_t total_blocks;
    uint32_t block_bitmap_start; // Offset in blocks
    uint32_t block_bitmap_size;  // Size in blocks
    uint32_t filetable_start;    // Offset in blocks
    uint32_t filetable_size;     // Size in blocks
    uint32_t data_start;         // Offset in blocks
    uint32_t checksum;
    uint32_t max_file_count;
    struct filetable_entry *filetable;
    char *block_bitmap;
};

struct portfs_disk_superblock {
    __be32 magic_number;
    __be32 block_size;
    __be32 total_blocks;
    __be32 block_bitmap_start;      // Offset in blocks
    __be32 block_bitmap_size;       // Size in blocks
    __be32 filetable_start;   // Offset in blocks
    __be32 filetable_size;    // Size in blocks
    __be32 data_start;        // Offset in blocks
    __be32 checksum;
    __be32 max_file_count;
};

// Variables
static uint64_t total_blocks = 0;

#endif // PORTFS_H
