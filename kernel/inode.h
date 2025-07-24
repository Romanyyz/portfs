#ifndef INODE_H
#define INODE_H

#include <linux/fs.h>

#include "shared_structs.h"

extern const struct inode_operations portfs_dir_inode_operations;
extern const struct inode_operations portfs_file_inode_operations;

struct inode *portfs_make_inode(struct super_block *sb, umode_t mode);
struct filetable_entry *portfs_find_free_file_entry(struct portfs_superblock *psb);
struct inode *portfs_get_inode_by_number(struct super_block *sb, uint32_t ino);

#endif // INODE_H
