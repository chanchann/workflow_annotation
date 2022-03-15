#! https://zhuanlan.zhihu.com/p/478396220
# workflow 源码解析 : 基础数据结构 rbtree

项目源码 : https://github.com/sogou/workflow

更加详细的源码注释可看 : https://github.com/chanchann/workflow_annotation

以下的代码源文件 : [code](https://github.com/chanchann/workflow_annotation/tree/main/demos/37_rb_tree)

我们来分析一下workflow中最为基础的数据结构 : rbtree, 是内核红黑树的写法

和上一节讲的list一样，每一个rb_node节点是嵌入在用RB树进行组织的数据结构中，而不是用rb_node指针进行数据结构的组织。

例如

```c
struct Task 
{
    int val;
    struct rb_node rb_node;
};
```

## 结构

```c
#pragma pack(1)
struct rb_node
{
	struct rb_node *rb_parent;
	struct rb_node *rb_right;
	struct rb_node *rb_left;
	char rb_color;
#define	RB_RED		0
#define	RB_BLACK	1
};
#pragma pack()
```

rb_node结构体描述了红黑树中的一个结点，这个结构体很自然，parent，right，left三个指针，还有一个`rb_color` 表示颜色

```c
struct rb_root
{
	struct rb_node *rb_node;
};
```

每个 rbtree 的根是一个 rb_root 结构，它通过以下的宏初始化为空：


```c
#define RB_ROOT (struct rb_root){ (struct rb_node *)0, } 
```

使用方式为

```c
struct rb_root task_tree = RB_ROOT;
```

我们展开来探究一下

```c
struct rb_root task_tree = (struct rb_root){ (struct rb_node *)0, };
```

内部的指针就初始化为null了


我们知道，树结构最为常见的三种操作: 1. 插入 2. 删除 3. 查找

## 插入

先看他的注释

```
To use rbtrees you'll have to implement your own insert and search cores.
This will avoid us to use callbacks and to drop drammatically performances.
I know it's not the cleaner way,  but in C (not in C++) to get
performances and genericity...

Some example of insert and search follows here. The search is a plain
normal search over an ordered tree. The insert instead must be implemented
int two steps: as first thing the code must insert the element in
order as a red leaf in the tree, then the support library function
rb_insert_color() must be called. Such function will do the
not trivial work to rebalance the rbtree if necessary.
```

为了性能，我们需要实现自己的insert和search功能。

首先我们实现一个insert功能

```c
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
```

我们简单解析一下

首先我们要找到在哪插入

```c
struct rb_node **tmp = &(root->rb_node), *parent = NULL;
```

我们这里就是先初始化一下，我们这里的tmp就是初始化指向root的rb_node结点，parent为空

```c
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
```

这里一个重点就是`rb_entry`, 这个其实就是和内核中大名鼎鼎的`container_of`是一个东西

```c
#define	rb_entry(ptr, type, member) \       
	((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member)))
```

我们看这个用法就知道

```c
struct Task *this = rb_entry(*tmp, struct Task, rb_node);
```

根据结构体中某成员变量的指针来求出指向整个结构体的指针。

结构体是一个线性存储的结构，无论在哪存放，我们这里的rb_node相对于整个结构体的地址的偏移值是不变的。

如果我们能够求出来这个偏移值的话，那么用rb_node的地址减去这个偏移值就是整个结构体struct task的地址。

我们慢慢拆封解析

```c
(type *)0  ==> 将0转换为TYPE类型的指针  ==> struct Task* (task)指向null 

((type *)0)->member  ==> 访问结构中的数据成员(rb_node) ==> task->node 

&(((TYPE *)0)->MEMBER) ==> 取出数据成员的地 ==> &task->node 

(unsigned long)(&((type *)0)->member) ==> 强制类型转换以后，其数值为结构体内的偏移量

((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member))) ==> 最后减去就得到了struct Task *this
```

接下来就简单了，小就走左边，大就走右边，直到走到叶子结点的左右支(nullptr)，因为二叉查找树中新插入的结点都是放在叶子结点上

接下来就是两个核心操作

```c
rb_link_node(&task->rb_node, parent, tmp);
rb_insert_color(&task->rb_node, root);
```

## rb_link_node

初始化新结点并插入： 

```c
static inline void rb_link_node(struct rb_node *node, struct rb_node *parent,
				struct rb_node **link)
{
	node->rb_parent = parent;    
	node->rb_color = RB_RED;     // 红黑树规定，插入的结点必定是红色的
	node->rb_left = node->rb_right = (struct rb_node *)0;   // 左右结点初始化为空

	*link = node;           // 这个*link就是*tmp，是我们刚才找到叶子结点最下面的一个分支(小就是left，大就是right)
}
```

## rb_insert_color

把新插入的节点进行着色，并且修正红黑树使其达到平衡

```c
void rb_insert_color(struct rb_node *node, struct rb_root *root)
{
	struct rb_node *parent, *gparent;

	// 关于插入操作的平衡调整，有两种特殊情况
	// 1. 如果插入的结点的父结点是黑色的，那我们什么都不做，它仍然满足红黑树定义(这种情况不进入while，do nothing)
	// 2. 如果插入的是根结点，那么我们直接改变颜色成黑色即可(所以我们这里的parent为null时,不进入while，最后一行变黑

	// 三种情况： 
	// 1）如果关注点node，它的叔叔结点d为红色 ，执行case1
	// 2）如果关注点node，它的叔叔结点d为黑色 ，且为父节点点右子节点，执行case2
	// 3）如果关注点node，它的叔叔结点d为黑色 ，且为父节点点左子节点，执行case3

	// case 1 ：
	// 操作是
	// 1）将关注点node的父节点，叔叔节点变黑
	// 2）将关注点node的祖父节点变红
	// 3）将关注点node变成祖父结点gparent
	// 4）跳转到case2 或者case3

	// case 2 
	// 1) 关注点变成父节点
	// 2）围绕这个新的关注点左旋
	// 3）跳到case 3

	// case 3 
	// 1）围绕关注点的祖父节点右旋
	// 2）将关注点的父节点和当前的兄弟节点互换颜色(其实就是父和祖交换)


	while ((parent = node->rb_parent) && parent->rb_color == RB_RED)
	{
		gparent = parent->rb_parent;

		if (parent == gparent->rb_left)
		{
			{
				register struct rb_node *uncle = gparent->rb_right;
				// case 1
				if (uncle && uncle->rb_color == RB_RED)
				{
					uncle->rb_color = RB_BLACK;   // 将关注点node的叔叔节点变黑
					parent->rb_color = RB_BLACK;  // 将关注点node的父节点变黑
					gparent->rb_color = RB_RED;   // 将关注点node的祖父节点变红
					node = gparent;				  // 将关注点node变成祖父结点gparent
					continue; 					 
				}
			}

			// 叔叔为黑，且是父亲右节点
			// case2
			if (parent->rb_right == node)    
			{
				register struct rb_node *tmp;
				__rb_rotate_left(parent, root);  
				tmp = parent;   
				parent = node;
				node = tmp;
			}

			// case3 
			// 这里先变父和祖父颜色，然后rotate，效果一样
			parent->rb_color = RB_BLACK;
			gparent->rb_color = RB_RED;
			__rb_rotate_right(gparent, root);
		} else {  
			// 相反的对称操作
		}
	}

	root->rb_node->rb_color = RB_BLACK;
}
```

这里的流程可以对着看看极客时间的[算法课](https://time.geekbang.org/column/100017301)

## 看看workflow如何使用insert

我们看看熟悉的poller中, 如何添加poller node节点

为了很好的和我们上面的`task_insert`做对比，进行风格上一点点小小改动

```c
static void __poller_tree_insert(struct __poller_node *node, poller_t *poller)
{
	struct rb_node **tmp = &poller->timeo_tree.rb_node, *parent = NULL;

	while (*tmp)
	{
		struct __poller_node *entry = rb_entry(*tmp, struct __poller_node, rb);
		
		parent = *tmp;

		if (__timeout_cmp(node, entry) < 0)
			tmp = &(*tmp)->rb_left;
		else
		{
			tmp = &(*tmp)->rb_right;
		}
	}

	...
	rb_link_node(&node->rb, parent, tmp);
	rb_insert_color(&node->rb, &poller->timeo_tree);
}
```

是不是一摸一样的操作

## 删除

```c
extern void rb_erase(struct rb_node *node, struct rb_root *root);
```

对于删除来说，虽然更为复杂，有二次调整，但是像魔方一样，有规则跳转可循，就不加赘述

而插入和查找才需要我们再封装，删除我们直接使用这个`rb_erase`即可

又看我们最为常见熟悉的poller

```c
static inline void __poller_tree_erase(struct __poller_node *node,
									   poller_t *poller)
{
	if (&node->rb == poller->tree_first)
		poller->tree_first = rb_next(&node->rb);

	rb_erase(&node->rb, &poller->timeo_tree);
	...
}
```

## 其他

其余的几个api很简单，看看实现注释也容易看懂

毕竟是个查找树，我们可以找到他逻辑上的前后，第一个，最后一个节点

```c
/* Find logical next and previous nodes in a tree */
extern struct rb_node *rb_next(struct rb_node *);
extern struct rb_node *rb_prev(struct rb_node *);
extern struct rb_node *rb_first(struct rb_root *);
extern struct rb_node *rb_last(struct rb_root *);
```

## 查找实现

其实和insert非常类似，二分查找，走左边还是右边

```c
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
```

## 完整的小demo

```c
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
```



