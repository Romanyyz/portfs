#include <linux/rbtree.h>

#define MAX_EXTENT_LENGTH 1024

struct free_extent {
    u32 start_block;
    u32 length;

    struct rb_node node;
};

struct portfs_superblock;

int portfs_build_extent_tree(struct portfs_superblock *psb, struct rb_root *free_extent_tree);
void portfs_destroy_extent_tree(struct rb_root *free_extent_tree);
void portfs_extent_tree_insert(struct rb_root *free_extent_tree, struct free_extent *new);
void portfs_extent_tree_remove(struct rb_root *free_extent_tree, struct free_extent *ext_to_remove);
bool portfs_extent_tree_empty(struct rb_root *free_extent_tree);
