#! https://zhuanlan.zhihu.com/p/415072416
# workflow 源码解析 02 : epoll 2

项目源码 : https://github.com/sogou/workflow

更加详细的源码注释可看 : https://github.com/chanchann/workflow_annotation

calltree.pl '(?i)epoll' '' 1 1 4

```
(?i)epoll
├── epoll_create
│   └── __poller_create_pfd   [vim src/kernel/poller.c +94]
├── epoll_ctl
│   ├── __poller_add_fd       [vim src/kernel/poller.c +99]
│   ├── __poller_del_fd       [vim src/kernel/poller.c +111]
│   ├── __poller_mod_fd       [vim src/kernel/poller.c +116]
│   └── __poller_add_timerfd  [vim src/kernel/poller.c +134]
└── epoll_wait
	└── __poller_wait [vim src/kernel/poller.c +158]
```

封装基本的三个操作，epoll_create， epoll_ctl， epoll_wait

## epoll_create

calltree.pl 'poller_create' '' 1 1 4

```
poller_create
└── __mpoller_create  [vim src/kernel/mpoller.c +24]
    └── mpoller_create        [vim src/kernel/mpoller.c +45]
        └── Communicator::create_poller       [vim src/kernel/Communicator.cc +1313]
            └── Communicator::init    [vim src/kernel/Communicator.cc +1341]
                ├── main      [vim tutorial/tutorial-13-kafka_cli.cc +182]
                ├── ParsedURI [vim _include/workflow/URIParser.h +51]
                ├── init      [vim _include/workflow/CommScheduler.h +57]
	...........
```

可见在 Communicator::init 中创建, 这里的 init 主要做了两件事，一件是 create_poller，一件是 create_handler_threads。

```cpp
int Communicator::init(size_t poller_threads, size_t handler_threads)
{
	...
	create_poller(poller_threads);
	create_handler_threads(handler_threads);
	return -1;
}
```

我们先看 create_poller

```cpp
int Communicator::create_poller(size_t poller_threads)
{
	struct poller_params params = 默认参数;

	msgqueue_create(4096, sizeof (struct poller_result));
	mpoller_create(&params, poller_threads);
	mpoller_start(this->mpoller);
}
```

可见 create_poller 完成这几件事 : msgqueue_create, mpoller_create, mpoller_start

继续深挖 poller，先不管 msgqueue

```cpp
mpoller_t *mpoller_create(const struct poller_params *params, size_t nthreads)
{
	size = offsetof(mpoller_t, poller) + nthreads * sizeof (void *);
	mpoller = (mpoller_t *)malloc(size);

	mpoller->nthreads = (unsigned int)nthreads;
	__mpoller_create(params, mpoller);

}
```

其中 mpoller_t 就是 \_\_mpoller

```cpp
/src/kernel/poller.h
typedef struct __mpoller mpoller_t;

struct __mpoller
{
	unsigned int nthreads;
	poller_t *poller[1];
};
```

mpoller_create 主要是给 mpoller 分配好大小，其中有一个是 poller\*的数组，然后调用\_\_mpoller_create() 利用 parmas 初始化

mpoller 职责就是(multi)批量生产

```cpp
static int __mpoller_create(const struct poller_params *params,
							mpoller_t *mpoller)
{
	...
	for (i = 0; i < mpoller->nthreads; i++)
	{
		mpoller->poller[i] = poller_create(params);
	}
}
```

而实际上，我们创建 poller 其实是初始化 poller_t

我们可以看他的内部结构

```cpp
typedef struct __poller poller_t;

struct __poller
{
	size_t max_open_files;
	poller_message_t *(*create_message)(void *);
	int (*partial_written)(size_t, void *);
	void (*cb)(struct poller_result *, void *);
	void *ctx;

	pthread_t tid;
	int pfd;
	int timerfd;
	int pipe_rd;
	int pipe_wr;
	int stopped;
	struct rb_root timeo_tree;
	struct rb_node *tree_first;
	struct list_head timeo_list;
	struct list_head no_timeo_list;
	struct __poller_node **nodes;
	pthread_mutex_t mutex;
	char buf[POLLER_BUFSIZE];
};
```

创建的时候就 malloc 分配空间，然后初始化赋值

```cpp
/src/kernel/poller.c

poller_t *poller_create(const struct poller_params *params)
{
	poller_t *poller = (poller_t *)malloc(sizeof (poller_t));
	n = params->max_open_files;
	poller->nodes = (struct __poller_node **)calloc(n, sizeof (void *));
	poller->pfd = __poller_create_pfd();
	__poller_create_timer(poller);
	pthread_mutex_init(&poller->mutex, NULL);

	poller->max_open_files = n;
	poller->create_message = params->create_message;
	poller->partial_written = params->partial_written;
	// callback			=	Communicator::callback,
	poller->cb = params->callback;
	poller->ctx = params->context;

	poller->timeo_tree.rb_node = NULL;
	poller->tree_first = NULL;
	INIT_LIST_HEAD(&poller->timeo_list);
	INIT_LIST_HEAD(&poller->no_timeo_list);
	poller->nodes[poller->timerfd] = POLLER_NODE_ERROR;
	poller->nodes[poller->pfd] = POLLER_NODE_ERROR;
	poller->stopped = 1;
	...
}
```

我们这里分配 max_open_files 这么多的 nodes

每一个 node 的结构为

```cpp
/src/kernel/poller.c

struct __poller_node
{
	int state;
	int error;
	struct poller_data data;
#pragma pack(1)
	union
	{
		struct list_head list;
		struct rb_node rb;
	};
#pragma pack()
	char in_rbtree;
	char removed;
	int event;
	struct timespec timeout;
	struct __poller_node *res;
};
```

这里一个非常重要的成员是 poller_data

```cpp
struct poller_data
{
#define PD_OP_READ			1
#define PD_OP_WRITE			2
#define PD_OP_LISTEN		3
#define PD_OP_CONNECT		4
#define PD_OP_SSL_READ		PD_OP_READ
#define PD_OP_SSL_WRITE		PD_OP_WRITE
#define PD_OP_SSL_ACCEPT	5
#define PD_OP_SSL_CONNECT	6
#define PD_OP_SSL_SHUTDOWN	7
#define PD_OP_EVENT			8
#define PD_OP_NOTIFY		9
#define PD_OP_TIMER			10
	short operation;
	unsigned short iovcnt;
	int fd;
	union
	{
		SSL *ssl;
		void *(*accept)(const struct sockaddr *, socklen_t, int, void *);
		void *(*event)(void *);
		void *(*notify)(void *, void *);
	};
	void *context;
	union
	{
		poller_message_t *message;
		struct iovec *write_iov;
		void *result;
	};
};
```

这里就到此创建好了 poller

## 启动 poller

上一章已经梳理过了怎么 start poller

我们退回到 create_poller 这里

create_poller -> mpoller_start -> poller_start -> pthread_create(&tid, NULL, \_\_poller_thread_routine, poller);

就启动了 poller 线程了

## 还需要解决的问题

代码梳理到此，还有许多不清楚的地方上述没讲到

比如 poller_t，\_\_poller_node，poller_data 为何这么设计

这个我们只能等怎么用才能知道这些结构的含义
