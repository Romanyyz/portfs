#ifndef PORTFS_H
#define PORTFS_H

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/types.h>
#include <linux/namei.h>
#include <linux/user_namespace.h>

#define PORTFS_BLOCK_SIZE 4096
#define MYFS_MAGIC 0x18181818
#define MAX_STORAGE_PATH 256

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
static int storage_init(void);

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
    int fileStartBlock;
    int sizeInBlocks;
    char name[64];
};

struct portfs_superblock {
    int block_size;
    int total_blocks;
    int bitmap_start;      // Offset in blocks
    int bitmap_size;       // Size in blocks
    int filetable_start;   // Offset in blocks
    int filetable_size;    // Size in blocks
    int data_start;        // Offset in blocks
    int data_blocks;
};

struct portfs_disk_superblock {
    __be32 block_size;
    __be32 total_blocks;
    __be32 bitmap_start;      // Offset in blocks
    __be32 bitmap_size;       // Size in blocks
    __be32 filetable_start;   // Offset in blocks
    __be32 filetable_size;    // Size in blocks
    __be32 data_start;        // Offset in blocks
    __be32 data_blocks;
};

// Variables
static uint64_t total_blocks = 0;
static char *block_bitmap;
static struct file* storage_filp = NULL;
static char storage_path[MAX_STORAGE_PATH] = { 0 };

#endif // PORTFS_H
