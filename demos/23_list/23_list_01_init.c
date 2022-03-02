#include "list.h"

// init demo

struct Task 
{
    struct list_head entry_list;
    int val;
};

int main() 
{
    struct Task task;
    INIT_LIST_HEAD(&task.entry_list);
    return 0;
}
