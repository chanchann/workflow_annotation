# workflow 源码解析 : 基础数据结构 rbtree

项目源码 : https://github.com/sogou/workflow

更加详细的源码注释可看 : https://github.com/chanchann/workflow_annotation

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

初始化新结点： 

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























