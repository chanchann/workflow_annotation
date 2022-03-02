#include "list.h"
#include <stdlib.h>
#include <stdio.h>

// list_del demo

struct Task 
{
    struct list_head entry_list;
    int val;
};

int main() 
{
    struct Task task;
    INIT_LIST_HEAD(&task.entry_list);
    struct list_head *pos;
    for(int i = 0; i < 4; i++)
    {
        struct Task *t = (struct Task *)malloc(sizeof(struct Task));
        t->val = i;
        list_add(&t->entry_list, &task.entry_list);
    }
    // 3 - 2 - 1 - 0
    list_for_each(pos, &task.entry_list) 
    {
        printf("val = %d\n", ((struct Task*)pos)->val);
    }

    struct Task *task_pos;
    list_for_each_entry(task_pos, &task.entry_list, entry_list) 
    {
        if (task_pos->val == 2) 
        {
            list_del(&task_pos->entry_list);
            break;
        }
    }

    printf("After del : \n");
    // 3 - 1 - 0
    list_for_each(pos, &task.entry_list) 
    {
        printf("val = %d\n", ((struct Task*)pos)->val);
    }

    return 0;
}
