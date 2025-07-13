#include "linux/types.h"

struct portfs_superblock;
struct filetable_entry;

int portfs_allocate_memory(struct portfs_superblock *psb,
                           struct filetable_entry *file_entry,
                           size_t bytes_to_allocate);
int portfs_alloc_indirect_extents(struct portfs_superblock *psb,
                                  struct filetable_entry *file_entry);
size_t portfs_get_allocated_size(const struct filetable_entry *entry,
                                 size_t block_size);
