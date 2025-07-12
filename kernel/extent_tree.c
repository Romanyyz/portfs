/*
 * extent_tree.c
 *
 * Implementation of a free extent red-black tree allocator.
 * Currently unused in this version of the filesystem.
 * Retained for potential future optimizations or advanced allocation logic.
 */

#include "extent_tree.h"

#include "linux/rbtree.h"
#include "linux/slab.h"

#include "shared_structs.h"

struct rb_root free_extent_tree = RB_ROOT;

void portfs_extent_tree_insert(struct free_extent *new)
{
    // TODO: Add lock for tree
    struct rb_node **link = &free_extent_tree.rb_node;
    struct rb_node *parent = NULL;

    while (*link)
    {
        struct free_extent *this = rb_entry(*link, struct free_extent, node);
        parent = *link;

        if (new->length < this->length)
            link = &(*link)->rb_left;
        else
            link = &(*link)->rb_right;
    }

    rb_link_node(&new->node, parent, link);
    rb_insert_color(&new->node, &free_extent_tree);
}


void portfs_extent_tree_remove(struct free_extent *ext_to_remove)
{
    // TODO: Add lock for tree
    rb_erase(&ext_to_remove->node, &free_extent_tree);
    kfree(ext_to_remove);
}


bool portfs_extent_tree_empty(void)
{
    return RB_EMPTY_ROOT(&free_extent_tree);
}


struct extent find_best_extent(u32 desired_length);
