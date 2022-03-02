#include "list.h"
#include <stdlib.h>
#include <stdio.h>

// list_move demo

struct Task 
{
    struct list_head entry_list;
    int val;
};

int main() 
{
    struct Task task1;
    INIT_LIST_HEAD(&task1.entry_list);

    struct Task task2;
    INIT_LIST_HEAD(&task2.entry_list);

    struct list_head *pos;
    for(int i = 0; i < 4; i++)
    {
        struct Task *t = (struct Task *)malloc(sizeof(struct Task));
        t->val = i;
        list_add(&t->entry_list, &task1.entry_list);
    }

    for(int i = 4; i < 8; i++)
    {
        struct Task *t = (struct Task *)malloc(sizeof(struct Task));
        t->val = i;
        list_add(&t->entry_list, &task2.entry_list);
    }

    struct Task *task_pos;
    list_for_each_entry(task_pos, &task1.entry_list, entry_list) 
    {
        if (task_pos->val == 2) 
        {
            list_move(&task_pos->entry_list, &task2.entry_list);
            break;
        }
    }

    printf("First link : \n");
    // 3 - 1 - 0
    list_for_each(pos, &task1.entry_list) 
    {
        printf("val = %d\n", ((struct Task*)pos)->val);
    }
    printf("Second link : \n");
    // 2 - 7 - 6 - 5 - 4
    list_for_each(pos, &task2.entry_list) 
    {
        printf("val = %d\n", ((struct Task*)pos)->val);
    }

    return 0;
}
