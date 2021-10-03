#! https://zhuanlan.zhihu.com/p/416556786
# workflow 源码解析 05 : msgqueue 

更加详细的源码注释可看 : https://github.com/chanchann/workflow_annotation

## msgqueueg接口

perl calltree.pl '(?i)msgqueue' '' 1 1 2

```
(?i)msgqueue
├── __msgqueue_swap
│   └── msgqueue_get  [vim src/kernel/msgqueue.c +102]
├── msgqueue_create
│   └── Communicator::create_poller   [vim src/kernel/Communicator.cc +1319]
├── msgqueue_destroy
│   ├── Communicator::create_poller   [vim src/kernel/Communicator.cc +1319]
│   ├── Communicator::init    [vim src/kernel/Communicator.cc +1356]
│   └── Communicator::deinit  [vim src/kernel/Communicator.cc +1380]
├── msgqueue_get
│   └── Communicator::handler_thread_routine  [vim src/kernel/Communicator.cc +1083]
├── msgqueue_put
│   └── Communicator::callback        [vim src/kernel/Communicator.cc +1256]
└── msgqueue_set_nonblock
    ├── Communicator::create_handler_threads  [vim src/kernel/Communicator.cc +1286]
    └── Communicator::deinit  [vim src/kernel/Communicator.cc +1380]
```

```cpp
msgqueue_t *msgqueue_create(size_t maxlen, int linkoff);
void msgqueue_put(void *msg, msgqueue_t *queue);
void *msgqueue_get(msgqueue_t *queue);
void msgqueue_set_nonblock(msgqueue_t *queue);
void msgqueue_set_block(msgqueue_t *queue);
void msgqueue_destroy(msgqueue_t *queue);
```

作为一个生产者消费者模型，我们最为核心的两个接口就是msgqueue_put和msgqueue_get。

我们重点讲解这两个接口及其在wf中是如何使用的

## 一个小demo

https://github.com/chanchann/workflow_annotation/blob/main/demos/25_msgque/25_msgque.cc

```cpp
int main()
{
    msgqueue_t *mq = msgqueue_create(10, -static_cast<int>(sizeof (void *)));
    char str[sizeof (void *) + 6];
    char *p = str + sizeof (void *);
    strcpy(p, "hello");
    msgqueue_put(p, mq);
    p = static_cast<char *>(msgqueue_get(mq));
    printf("%s\n", p);
    return 0;
}
```

## msgqueue_create - 先初始化(需要注意linkoff)

我们上一节看到的 create_poller

```cpp
int Communicator::create_poller(size_t poller_threads)
{
	struct poller_params params = 默认参数;

	msgqueue_create(4096, sizeof (struct poller_result));
	mpoller_create(&params, poller_threads);
	mpoller_start(this->mpoller);
}
```

create_poller 完成这几件事 : msgqueue_create, mpoller_create, mpoller_start

我们已经知道如何创建poller并启动，现在来看看创建msgqueue

这里就是分配空间，初始化

```cpp
msgqueue_t *msgqueue_create(size_t maxlen, int linkoff)
{
	msgqueue_t *queue = (msgqueue_t *)malloc(sizeof (msgqueue_t));

	pthread_mutex_init(&queue->get_mutex, NULL)
	pthread_mutex_init(&queue->put_mutex, NULL);
	pthread_cond_init(&queue->get_cond, NULL);
	pthread_cond_init(&queue->put_cond, NULL);

	queue->msg_max = maxlen;
	queue->linkoff = linkoff;
	queue->head1 = NULL;
	queue->head2 = NULL;
	queue->get_head = &queue->head1;
	queue->put_head = &queue->head2;
	queue->put_tail = &queue->head2;
	queue->msg_cnt = 0;
	queue->nonblock = 0;
	...
}
```

这里比较重要的就是linkoff，我们在msgqueue_put可以看出他的作用

```cpp
typedef struct __msgqueue msgqueue_t;

// 消息队列就是个单链表
// 此处有两个链表，高效swap使用
struct __msgqueue
{
	size_t msg_max;
	size_t msg_cnt;
	int linkoff;
	int nonblock;
	void *head1;     // get_list   
	void *head2;     // put_list
	// 两个list，高效率，一个在get_list拿，一个在put_list放
	// 如果get_list空，如果put_list放了的话，那么swap一下就可了，O(1),非常高效，而且互不干扰
	void **get_head;	
	void **put_head;
	void **put_tail;
	pthread_mutex_t get_mutex;
	pthread_mutex_t put_mutex;
	pthread_cond_t get_cond;
	pthread_cond_t put_cond;
};
```

## msgqueue_put, put - 生产者

就是把epoll收到的消息队列加入到消息队列中

```cpp
void Communicator::callback(struct poller_result *res, void *context)
{
	Communicator *comm = (Communicator *)context;
	msgqueue_put(res, comm->queue);
}
```

```cpp
void msgqueue_put(void *msg, msgqueue_t *queue)
{
	// 这里转char* 是因为，void* 不能加减运算，但char* 可以
	void **link = (void **)((char *)msg + queue->linkoff);
	/*
	this->queue = msgqueue_create(4096, sizeof (struct poller_result));
	初始化的时候把linkoff大小设置成了sizeof (struct poller_result)
	*/
	// msg头部偏移linkoff字节，是链表指针的位置。使用者s需要留好空间。这样我们就无需再malloc和free了
	// 我们就是把一个个的struct poller_result 串起来
	*link = NULL; 

	pthread_mutex_lock(&queue->put_mutex);
	
	// 当收到的cnt大于最大限制 且 阻塞mode(default)， 那么wait在这, 等待消费者去给消费了
	while (queue->msg_cnt > queue->msg_max - 1 && !queue->nonblock)
		pthread_cond_wait(&queue->put_cond, &queue->put_mutex);

	*queue->put_tail = link;  // 把 link串到链尾
	queue->put_tail = link;   // 然后把这个指针移过来

	queue->msg_cnt++;
	pthread_mutex_unlock(&queue->put_mutex);

	pthread_cond_signal(&queue->get_cond);
}
```

这个函数就是把msg添加到了queue后面串起来

## msgqueue_get - get ：消费者

```cpp
void Communicator::handler_thread_routine(void *context)
{
	...
	while ((res = (struct poller_result *)msgqueue_get(comm->queue)) != NULL)
	{
		switch (res->data.operation)
		{
		case PD_OP_READ:
			comm->handle_read_result(res);
			break;
		...
		}
	}
}
```

msqqueue是epoll消息回来之后，以网络线程作为生产者往queue里放(上面`msgqueue_put(res, comm->queue);`)

执行线程作为消费者从queue里拿数据，从而做到线程互不干扰

```cpp
void *msgqueue_get(msgqueue_t *queue)
{
	pthread_mutex_lock(&queue->get_mutex);

	// 如果get_list有消息
	// 若get_list无消息了，那么看看put_list有没有，如果有，swap一下即可
	if (*queue->get_head || __msgqueue_swap(queue) > 0)
	{
		// *queue->get_head 是第一个
		// 转换为(char *)可做加减法
		// 其中保留了linkoff这么大的空间
		// this->queue = msgqueue_create(4096, sizeof (struct poller_result));
		// 初始化的时候把linkoff大小设置成了sizeof (struct poller_result)
		// 退回后就是msg的起始位置了
		msg = (char *)*queue->get_head - queue->linkoff;
		// *queue->get_head就是第一个元素
		// *(void **)*queue->get_head 就是第一个元素指向的下一个元素
		// 第一个元素移动过来
		*queue->get_head = *(void **)*queue->get_head;
	}
	else
	{
		msg = NULL;
		errno = ENOENT;
	}

	pthread_mutex_unlock(&queue->get_mutex);
	return msg;
}
```

这里还有个非常重要的细节__msgqueue_swap

我们两个list，一个在get_list拿，一个在put_list放

如果get_list空，如果put_list放了的话，那么swap一下就可了，O(1),非常高效，而且互不干扰

```cpp
static size_t __msgqueue_swap(msgqueue_t *queue)
{
	void **get_head = queue->get_head;
	size_t cnt;
	// 将get_head切换好，因为就算put在加，put_head也不会变, 所以不需要加锁
	queue->get_head = queue->put_head;  

	pthread_mutex_lock(&queue->put_mutex);

	// 如果put_list也没有消息且为阻塞态，那么就wait等到放进来消息
	while (queue->msg_cnt == 0 && !queue->nonblock)
		pthread_cond_wait(&queue->get_cond, &queue->put_mutex);

	cnt = queue->msg_cnt;  
	// 如果cnt大于最大接收的msg，那么通知put，因为大于msg_max put_list wait在那里了，所以swap清空了就要唤醒生产者put
	if (cnt > queue->msg_max - 1)
		pthread_cond_broadcast(&queue->put_cond);

	queue->put_head = get_head;    // put_list就交换设置到get_list那个地方了  
	queue->put_tail = get_head;

	// put_list清0了
	// 收到put消息是queue->msg_cnt++, 并没有拿走消息queue->msg_cnt--;
	// 靠的就是put_list swap 到 get_list 就清0了
	queue->msg_cnt = 0;    

	pthread_mutex_unlock(&queue->put_mutex);
	return cnt;
}
```



