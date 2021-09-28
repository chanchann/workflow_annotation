#include "list.h"
#include <stdio.h>
#include <vector>
#include <cassert>

struct Task {
    struct list_head entry_list;
    int val;
};

void test01() {
    fprintf(stderr, "test01\n");
    // struct list_head list_test = LIST_HEAD_INIT(list_test);
    
    LIST_HEAD(list_test);
    
    assert(list_empty(&list_test));

    // 这里的head只是dummy head
    Task task_1;
    task_1.val = 1;
    Task task_2;
    task_2.val = 2;
    // head - 1
    list_add_tail(&task_1.entry_list, &list_test);
    // head - 1 - 2
    // 往tail后面加
    list_add_tail(&task_2.entry_list, &list_test);

	struct list_head *pos, *tmp;
	struct Task *entry;

	list_for_each_safe(pos, tmp, &list_test)
	{
		entry = list_entry(pos, struct Task, entry_list);
        fprintf(stderr, "entry : %d\n", entry->val);
	}   
    /*
    list_for_each_safe(pos, tmp, &list_test) 
    equals
    for (pos = (&list_test)->next, tmp = pos->next; pos != (&list_test); pos = tmp, tmp = pos->next)
    */
}

void test02() {
    fprintf(stderr, "test02\n");
    struct list_head list_test = LIST_HEAD_INIT(list_test);
    // struct list_head list_test = { &(list_test), &(list_test) }  
    // 相当于隐式构造了，赋值了两个指针，一个prev，一个next
}

void test03() {
    fprintf(stderr, "test03\n");
    LIST_HEAD(list_test);
    Task task_1;
    task_1.val = 1;
    Task task_2;
    task_2.val = 2;
    
    // head - 2 
    list_add(&task_1.entry_list, &list_test);
    // head - 2 - 1
    list_add(&task_2.entry_list, &list_test);
    // 就是往dummy head 后面挂着

	struct list_head *pos, *tmp;
	struct Task *entry;

	list_for_each_safe(pos, tmp, &list_test)
	{
		entry = list_entry(pos, struct Task, entry_list);
        fprintf(stderr, "entry : %d\n", entry->val);
	}   

    list_del(&task_2.entry_list);

	list_for_each_safe(pos, tmp, &list_test)
	{
		entry = list_entry(pos, struct Task, entry_list);
        fprintf(stderr, "entry : %d\n", entry->val);
	}      
}


// todo : 此处有问题，再仔细画图check check
void test04() {
    fprintf(stderr, "test04\n");
    LIST_HEAD(list_test);
    std::vector<Task> task_vec;
    for(int i = 0; i < 10; i++) {
        Task task;  
        task.val = i;
        task_vec.push_back(task);
        // 注意临时变量的生命周期
        list_add(&task_vec.back().entry_list, &list_test);
        // fprintf(stderr, "val : %d , addr : %p, entry_list : %p, %p\n",  
            // task_vec.back().val, &task_vec.back(), task_vec.back().entry_list);
    }
    
	struct list_head *pos, *tmp;
	Task *entry;
	list_for_each_safe(pos, tmp, &list_test)
	{
		entry = list_entry(pos, Task, entry_list);
        fprintf(stderr, "entry : %d\n", entry->val);
	}   

    // another list 
    LIST_HEAD(list_test2);
    for(int i = 1; i <= 4; i++) {
        // fprintf(stderr, "test : %d\n", task_vec[i].val);
        list_move(&task_vec[i].entry_list, &list_test2);
    }

    // todo : 此处有问题
    // head - 0 - 1 - 5 - 6 - ...
    fprintf(stderr, "list1\n");
    list_for_each(pos, &list_test) 
    {    
		entry = list_entry(pos, Task, entry_list);
        fprintf(stderr, "entry : %d\n", entry->val);
	}   
	// list_for_each_safe(pos, tmp, &list_test)
	// {    
	// 	entry = list_entry(pos, Task, entry_list);
    //     fprintf(stderr, "entry : %d\n", entry->val);
	// }   

    // head - 4 - 3 - 2
    fprintf(stderr, "list2\n");
    list_for_each_safe(pos, tmp, &list_test2)
	{    
		entry = list_entry(pos, Task, entry_list);
        fprintf(stderr, "entry : %d\n", entry->val);
	}   
}

int main() {
    test04();

}