# workflow 源码解析 05 : msgqueue 

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

```cpp
typedef struct __msgqueue msgqueue_t;

struct __msgqueue
{
	size_t msg_max;
	size_t msg_cnt;
	int linkoff;
	int nonblock;
	void *head1;
	void *head2;
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

只在一处出现，就是把epoll收到的消息队列加入到消息队列中

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
	void **link = (void **)((char *)msg + queue->linkoff);

	*link = NULL;

	pthread_mutex_lock(&queue->put_mutex);

	while (queue->msg_cnt > queue->msg_max - 1 && !queue->nonblock)
		pthread_cond_wait(&queue->put_cond, &queue->put_mutex);

	*queue->put_tail = link;
	queue->put_tail = link;

	queue->msg_cnt++;
	pthread_mutex_unlock(&queue->put_mutex);

	pthread_cond_signal(&queue->get_cond);
}
```

这个函数就是把msg添加到了queue后面串起来

如果消息数量大于最大或者queue不是非阻塞，那么就wait。

## msgqueue_get - get ：消费者

只出现在一处

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
	...
	pthread_mutex_lock(&queue->get_mutex);

	if (*queue->get_head || __msgqueue_swap(queue) > 0)
	{
		msg = (char *)*queue->get_head - queue->linkoff;
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





