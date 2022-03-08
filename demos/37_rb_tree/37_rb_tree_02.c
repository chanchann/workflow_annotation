// rb_node是节点类型
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

// rb_root是仅包含一个节点指针的类，用来表示根节点。
struct rb_root
{
	struct rb_node *rb_node;
};

#define RB_ROOT (struct rb_root){ (struct rb_node *)0, }    // 初始根节点指针

struct Task 
{
    int val;
    struct rb_node rb_node;
};

int main()
{
    struct rb_root task_tree = RB_ROOT;
    
}