#include "rbtree.h"
#include <stdlib.h>

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
    

}