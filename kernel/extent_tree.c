/*
 *
 * Implementation of a free extent red-black tree allocator.
 */
#include "extent_tree.h"

#include "linux/rbtree.h"
#include "linux/slab.h"

#include "shared_structs.h"
#include "bitmap.h"

int portfs_build_extent_tree(struct portfs_superblock *psb,
                             struct rb_root *free_extent_tree)
{
    const u32 total_blocks = psb->total_blocks;
    u32 length = 0;

    for (int i = psb->data_start; i < total_blocks; ++i)
    {
        if (!is_block_allocated(psb->block_bitmap, i) && length < MAX_EXTENT_LENGTH)
        {
            length++;
        }
        else
        {
            if (length > 0) {
                struct free_extent *free_ext = kmalloc(sizeof(*free_ext), GFP_KERNEL);
                if (!free_ext)
                    return -ENOMEM;

                free_ext->length = length;
                free_ext->start_block = i - length;
                portfs_extent_tree_insert(free_extent_tree, free_ext);
                length = 0;
            }
        }
    }
    if (length > 0)
    {
        struct free_extent *free_ext = kmalloc(sizeof(*free_ext), GFP_KERNEL);
        if (!free_ext)
            return -ENOMEM;

        free_ext->length = length;
        free_ext->start_block = total_blocks - length;
        portfs_extent_tree_insert(free_extent_tree, free_ext);
    }

    return 0;
}


void portfs_destroy_extent_tree(struct rb_root *free_extent_tree)
{
    struct rb_node *node, *next;
    for (node = rb_first(free_extent_tree); node; node = next)
    {
        next = rb_next(node);
        rb_erase(node, free_extent_tree);
        struct free_extent *data = rb_entry(node, struct free_extent, node);
        kfree(data);
    }
}


void portfs_extent_tree_insert(struct rb_root *free_extent_tree, struct free_extent *new)
{
    struct rb_node **link = &(free_extent_tree->rb_node);
    struct rb_node *parent = NULL;

    while (*link)
    {
        struct free_extent *this = rb_entry(*link, struct free_extent, node);
        parent = *link;

        if (new->length > this->length)
            link = &(*link)->rb_left;
        else if (new->length < this->length)
            link = &(*link)->rb_right;
        else if (new->start_block < this->start_block)
            link = &(*link)->rb_left;
        else
            link = &(*link)->rb_right;
    }

    rb_link_node(&new->node, parent, link);
    rb_insert_color(&new->node, free_extent_tree);
}


void portfs_extent_tree_remove(struct rb_root *free_extent_tree, struct free_extent *ext_to_remove)
{
    rb_erase(&ext_to_remove->node, free_extent_tree);
    kfree(ext_to_remove);
}


bool portfs_extent_tree_empty(struct rb_root *free_extent_tree)
{
    return RB_EMPTY_ROOT(free_extent_tree);
}
