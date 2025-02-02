#ifndef INODE_H
#define INODE_H

#include <linux/fs.h>

extern const struct inode_operations portfs_dir_inode_operations;
extern const struct inode_operations portfs_file_inode_operations;

#endif // INODE_H
