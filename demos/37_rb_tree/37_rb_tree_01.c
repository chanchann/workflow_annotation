#include "rbtree.h"
#include <stdlib.h>
#include <stdio.h>

#define TASK_NUM 18

struct Task 
{
    int val;
    struct rb_node rb_node;
};

int task_insert(struct rb_root *root, struct Task *task)
{
    struct rb_node **tmp = &(root->rb_node), *parent = NULL;
 
    /* Figure out where to put new node */
    while (*tmp) {
        struct Task *this = rb_entry(*tmp, struct Task, rb_node);
    
        parent = *tmp;
        if (task->val < this->val)
            tmp = &((*tmp)->rb_left);
        else if (task->val > this->val)
            tmp = &((*tmp)->rb_right);
        else 
            return -1;
    }
    
    /* Add new node and rebalance tree. */
    rb_link_node(&task->rb_node, parent, tmp);
    rb_insert_color(&task->rb_node, root);
    
    return 0;
}

struct Task *task_search(struct rb_root *root, int val)
{
 	struct rb_node *node = root->rb_node;

    while (node) {
        struct Task *data = rb_entry(node, struct Task, rb_node);
    
        if (val < data->val)
            node = node->rb_left;
        else if (val > data->val)
            node = node->rb_right;
        else 
            return data;
    }
    
    return NULL;
}

void task_free(struct Task *node)
{
	if (node) { 
		free(node);
		node = NULL;
	}
}

int main()
{
    struct rb_root task_tree = RB_ROOT;

    struct Task *task_list[TASK_NUM];
    // insert
    int i = 0;
	for (; i < TASK_NUM; i++) {
		task_list[i] = (struct Task *)malloc(sizeof(struct Task));
		task_list[i]->val = i;
        task_insert(&task_tree, task_list[i]);
	}

    // traverse
	struct rb_node *node;
	printf("traverse all nodes: \n");
	for (node = rb_first(&task_tree); node; node = rb_next(node))
		printf("val = %d\n", rb_entry(node, struct Task, rb_node)->val);

    // delete
	struct Task *data = task_search(&task_tree, 10);
	if (data) {
		rb_erase(&data->rb_node, &task_tree);
		task_free(data);
	}

    // traverse again
	printf("traverse again: \n");
	for (node = rb_first(&task_tree); node; node = rb_next(node))
		printf("val = %d\n", rb_entry(node, struct Task, rb_node)->val);
}