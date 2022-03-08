#! https://zhuanlan.zhihu.com/p/474833945
# workflow 源码解析 : 基础数据结构 list 

项目源码 : https://github.com/sogou/workflow

更加详细的源码注释可看 : https://github.com/chanchann/workflow_annotation

以下的代码源文件 : [code](https://github.com/chanchann/workflow_annotation/tree/main/demos/23_list)

我们来分析一下workflow中最为基础的数据结构 : list, 是内核链表的写法，如果不熟悉内核list的操作，很多workflow的操作也会一头雾水

## 内核链表的结构

```c
struct list_head {
	struct list_head *next, *prev;
};
```

首先我们看看他的结构，只有前驱和后继指针，并不包含数据域。

当需要用内核的链表结构时，只需要在数据结构体中定义一个struct list_head{}类型的结构体成员对象就可以。

比如:

```c
struct Task {
    struct list_head entry_list;
    int val;
};
```

## 链表的初始化

```c
#define LIST_HEAD_INIT(name) { &(name), &(name) }

#define LIST_HEAD(name) \
	struct list_head name = LIST_HEAD_INIT(name)

static inline void INIT_LIST_HEAD(struct list_head *list)
{
	list->next = list;
	list->prev = list;
}
```

INIT_LIST_HEAD 和LIST_HEAD都可以初始化链表。

但是 LIST_HEAD（task_list） 初始化链表时会顺便创建链表对象。

我们来展开看看:

```c
LIST_HEAD（task_list）

==> struct list_head task_list = LIST_HEAD_INIT(task_list)

==> struct list_head task_list = { &(task_list), &(task_list) }

==> task_list->next = &task_list / task_list->prev = &task_list;
```

链表的初始化其实非常简单，就是让链表的前驱和后继都指向了自己。

在这展开的过程中我们也可以看到INIT_LIST_HEAD的用法，不过是需要我们已经有了一个链表对象task_list

```c
#include "list.h"

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
```


## 链表增加节点 : list_add

```c
static inline void __list_add(struct list_head *node,
							  struct list_head *prev,
							  struct list_head *next)
{
	next->prev = node;
	node->next = next;
	node->prev = prev;
	prev->next = node;
}

static inline void list_add(struct list_head *node, struct list_head *head)
{
	__list_add(node, head, head->next);
}

static inline void list_add_tail(struct list_head *node,
								 struct list_head *head)
{
	__list_add(node, head->prev, head);
}
```

list_add 是头插法，1->2->3,使用list_add插入4后变为，4->1->2->3

list_add_tail为尾插法， 1->2->3,使用list_add_tail插入4后变为，1->2->3->4

```c
#include "list.h"
#include <stdlib.h>
#include <stdio.h>

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
    list_for_each(pos, &task.entry_list) {
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
```

## 删除节点 : list_del

```c
static inline void __list_del(struct list_head *prev, struct list_head *next)
{
	next->prev = prev;
	prev->next = next;
}

static inline void list_del(struct list_head *entry)
{
	__list_del(entry->prev, entry->next);
}
```

```c
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
```

## 链表删除并插入节点 : list_move

```c
static inline void list_move(struct list_head *list, struct list_head *head)
{
	__list_del(list->prev, list->next);
	list_add(list, head);
}

static inline void list_move_tail(struct list_head *list,
								  struct list_head *head)
{
	__list_del(list->prev, list->next);
	list_add_tail(list, head);
}
```

从实现也看出，list_move是删除list指向的节点，同时将其以头插法插入到head中。

```c
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
```

## 链表的合并 : list_splice

```c
static inline void __list_splice(struct list_head *list,
								 struct list_head *head)
{
	struct list_head *first = list->next;
	struct list_head *last = list->prev;
	struct list_head *at = head->next;

	first->prev = head;
	head->next = first;

	last->next = at;
	at->prev = last;
}

static inline void list_splice(struct list_head *list, struct list_head *head)
{
	if (!list_empty(list))
		__list_splice(list, head);
}

static inline void list_splice_init(struct list_head *list,
									struct list_head *head)
{
	if (!list_empty(list)) {
		__list_splice(list, head);
		INIT_LIST_HEAD(list);
	}
}
```

```c
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

    // 3 - 2 - 1 - 0 - 7 - 6 - 5 - 4
    list_for_each(pos, &task2.entry_list) 
    {
        printf("val = %d\n", ((struct Task*)pos)->val);
    }

    return 0;
}
```

我们这里说明一下

当前有两个链表，表头分别是head1和head2（都是struct list_head变量

当调用list_splice(&head1, &head2)时，注意这里是把第一个链表往第二个链表上挂在合并，然后第一个链表的头作为首节点，而尾节点不变。

```c
static inline void __list_splice(struct list_head *list,
								 struct list_head *head)
{
	struct list_head *first = list->next;
	struct list_head *last = list->prev;
	struct list_head *at = head->next;

	first->prev = head;
	head->next = first;

	last->next = at;
	at->prev = last;
}

我们简单分析下这个代码就可知道

假设两个链表

1 - 2 - 3

4 - 5

第一个链表的首，first为1， last为3

第二个链表的首为at = 4

1->prev = head

head->next = 1

那么这里就是把1当成第一个插入了

3->next = 4

4->prev = 3

那么这里就是把4续接到3上了
```

## 链表的遍历

```c
#define list_for_each(pos, head) \
	for (pos = (head)->next; pos != (head); pos = pos->next)
```

之前我们用过这个来遍历打印

```
list_for_each(pos, &task2.entry_list) 
{
    printf("val = %d\n", ((struct Task*)pos)->val);
}

==> 展开

struct list_head *pos;
head = &task2.entry_list
for (pos = (head)->next; pos != (head); pos = pos->next)
{
    printf("val = %d\n", ((struct Task*)pos)->val);
}
```

很好理解，就是个for循环，需要一个curosr，一个list的地址

list_for_each_prev / list_for_each_safe 等等同理

## 获取节点结构体的地址

```c
#define list_entry(ptr, type, member) \
	((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member)))
```
   
这个宏从一个结构的成员指针找到其容器的指针

我们先看上述用到了一个宏

```c
struct Task *task_pos;
list_for_each_entry(task_pos, &task.entry_list, entry_list) 
{
    if (task_pos->val == 2) 
    {
        list_del(&task_pos->entry_list);
        break;
    }
}
```

```c
#define list_for_each_entry(pos, head, member) \
	for (pos = list_entry((head)->next, typeof (*pos), member); \
		 &pos->member != (head); \
		 pos = list_entry(pos->member.next, typeof (*pos), member))
```

我们这里展开就是

```c
for (task_pos = list_entry((&task.entry_list)->next, typeof (*task_pos), entry_list);
     &task_pos->entry_list != (&task.entry_list); 
     task_pos = list_entry(task_pos->entry_list.next, typeof (*task_pos), entry_list))
{
    xxxx
}
```

这里本质上就是用到了一个循环，核心还是用`list_entry`，我们继续展开

```c
struct Task *task_pos = ((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member)))

这里的参数 ptr, type, member分别是

ptr - (&task.entry_list)->next

这里的 ptr是找容器的那个变量的指针，这可以理解，第一次调用就是list中第一个entry_list *这个指针

(char*)ptr是为了计算字节偏移

type - typeof (*task_pos)

ANSI C标准允许值为0的常量被强制转换成任何一种类型的指针，并且转换的结果是个NULL，因此((type *)0)的结果就是一个类型为type *的NULL指针.

如果利用这个NULL指针来访问type的成员当然是非法的，但typeof( ((type *)0)->member )是想取该成员的类型，编译器不会生成访问type成员的代码

&( ((type *)0)->member )在最前面有个取地址符&，它的意图是想取member的地址，所以编译器同样会优化为直接取地址。那我们可取到以“0”为基地址的一个type型变量member域的地址。那么这个地址也就等于member域到结构体基地址的偏移字节数。

(char *)(ptr)使得指针的加减操作步长为一字节，(unsigned long)(&((type *)0)->member)等于ptr指向的member到该member所在结构体基地址的偏移字节数。二者一减便得出该结构体的地址。转换为 (type *)型的指针

member - entry_list

==> 

struct Task *task_pos = ((struct Task *)((char *)(给的entry_list)-(unsigned long)(&((struct Task *)0)->entry_list)))
```



