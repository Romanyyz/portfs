#include <linux/rbtree.h>

struct free_extent {
    u32 start_block;
    u32 length;

    struct rb_node node;
};

extern struct rb_root free_extent_tree;

void portfs_extent_tree_insert(struct free_extent *new);
void portfs_extent_tree_remove(struct free_extent *ext_to_remove);
struct extent portfs_find_best_extent(u32 desired_length);
bool portfs_extent_tree_empty(void);
