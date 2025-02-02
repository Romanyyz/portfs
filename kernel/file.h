#ifndef FILE_H
#define FILE_H

#include <linux/fs.h>

extern const struct file_operations portfs_dir_file_operations;
extern const struct file_operations portfs_file_operations;

#endif // FILE_H
