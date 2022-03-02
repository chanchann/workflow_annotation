#include "list.h"
#include <stdlib.h>
#include <stdio.h>

// list_splice demo

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

    printf("First link : \n");
    // 3 - 2 - 1 - 0
    list_for_each(pos, &task1.entry_list) 
    {
        printf("val = %d\n", ((struct Task*)pos)->val);
    }
    printf("Second link : \n");
    // 7 - 6 - 5 - 4
    list_for_each(pos, &task2.entry_list) 
    {
        printf("val = %d\n", ((struct Task*)pos)->val);
    }
    printf("After join two linklist : \n");
    list_splice(&task1.entry_list, &task2.entry_list);

    // infinite loop here
    // list_for_each(pos, &task1.entry_list) 
    // {
    //     printf("val = %d\n", ((struct Task*)pos)->val);
    // }
    // printf("Second link : \n");

    // 3 - 2 - 1 - 0 - 7 - 6 - 5 - 4
    list_for_each(pos, &task2.entry_list) 
    {
        printf("val = %d\n", ((struct Task*)pos)->val);
    }

    return 0;
}
