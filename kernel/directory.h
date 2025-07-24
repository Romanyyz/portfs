#ifndef DIRECTORY_H
#define DIRECTORY_H

#include <linux/types.h>
#include <linux/stat.h>

#include "shared_structs.h"

#define MAX_NAME_LENGTH 64

struct dir_entry
{
    char name[MAX_NAME_LENGTH];
    uint32_t inode_number;
};

struct disk_dir_entry
{
    char name[MAX_NAME_LENGTH];
    __be32 inode_number;
} __attribute__((packed));


static inline bool is_directory(const struct filetable_entry *fe)
{
    return S_ISDIR(fe->mode);
}


static inline struct extent *get_direct_extents(struct filetable_entry *fe)
{
    return is_directory(fe) ? NULL : fe->file.direct_extents;
}


static inline uint32_t get_dir_block(struct filetable_entry *fe)
{
    return is_directory(fe) ? fe->dir.dir_block : 0;
}


static inline void set_dir_block(struct filetable_entry *fe, uint32_t dir_block)
{
    if (is_directory(fe))
        fe->dir.dir_block = dir_block;
}


static inline int get_max_dir_entries(struct portfs_superblock *psb)
{
    uint32_t disk_size = psb->block_size;
    uint16_t num_entries = disk_size / sizeof(struct disk_dir_entry);
    return num_entries;
}

int portfs_de_add(struct portfs_superblock *psb,
                  struct filetable_entry *parent_dir,
                  struct dir_entry *dir_entry);

struct dir_entry *portfs_de_remove(struct portfs_superblock *psb,
                                   struct filetable_entry *parent_dir,
                                   const char *name);

struct inode *portfs_create_dir(struct inode *dir,
                                struct dentry *dentry,
                                umode_t mode);

struct dir_entry *portfs_de_find(struct portfs_superblock *psb,
                                 struct filetable_entry *parent_dir,
                                 const char *name);

bool portfs_is_dir_empty(struct portfs_superblock *psb, struct filetable_entry *dir);

int portfs_load_dir_data(struct portfs_superblock *psb,
                         struct filetable_entry *parent_dir);

#endif //DIRECTORY_H
