#include "list.h"
#include <stdlib.h>
#include <stdio.h>

// list_add demo

struct Task 
{
    struct list_head entry_list;
    int val;
};

void list_add_test()
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
}

void list_tail_test()
{
    struct Task task;
    INIT_LIST_HEAD(&task.entry_list);
    struct list_head *pos;
    for(int i = 0; i < 4; i++)
    {
        struct Task *t = (struct Task *)malloc(sizeof(struct Task));
        t->val = i;
        list_add_tail(&t->entry_list, &task.entry_list);
    }
    // 0 - 1 - 2 - 3
    list_for_each(pos, &task.entry_list) 
    {
        printf("val = %d\n", ((struct Task*)pos)->val);
    }
}

int main() 
{
    printf("list_add_test : \n");
    list_add_test();

    printf("list_tail_test : \n");
    list_tail_test();
    return 0;
}
