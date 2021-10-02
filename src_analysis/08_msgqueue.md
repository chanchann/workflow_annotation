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

我们还是先来看看msgqueue的接口

```cpp
msgqueue_t *msgqueue_create(size_t maxlen, int linkoff);
void msgqueue_put(void *msg, msgqueue_t *queue);
void *msgqueue_get(msgqueue_t *queue);
void msgqueue_set_nonblock(msgqueue_t *queue);
void msgqueue_set_block(msgqueue_t *queue);
void msgqueue_destroy(msgqueue_t *queue);
```

## 


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

我们已经知道如何创建poller并启动，现在来看看msgqueue

```cpp
msgqueue_t *msgqueue_create(size_t maxlen, int linkoff)
{
	msgqueue_t *queue = (msgqueue_t *)malloc(sizeof (msgqueue_t));
	int ret;

	if (!queue)
		return NULL;

	ret = pthread_mutex_init(&queue->get_mutex, NULL);
	if (ret == 0)
	{
		ret = pthread_mutex_init(&queue->put_mutex, NULL);
		if (ret == 0)
		{
			ret = pthread_cond_init(&queue->get_cond, NULL);
			if (ret == 0)
			{
				ret = pthread_cond_init(&queue->put_cond, NULL);
				if (ret == 0)
				{
					queue->msg_max = maxlen;
					queue->linkoff = linkoff;
					queue->head1 = NULL;
					queue->head2 = NULL;
					queue->get_head = &queue->head1;
					queue->put_head = &queue->head2;
					queue->put_tail = &queue->head2;
					queue->msg_cnt = 0;
					queue->nonblock = 0;
					return queue;
				}

				pthread_cond_destroy(&queue->get_cond);
			}

			pthread_mutex_destroy(&queue->put_mutex);
		}

		pthread_mutex_destroy(&queue->get_mutex);
	}

	errno = ret;
	free(queue);
	return NULL;
}
```

```cpp
struct poller_result
{
#define PR_ST_SUCCESS		0
#define PR_ST_FINISHED		1
#define PR_ST_ERROR			2
#define PR_ST_DELETED		3
#define PR_ST_MODIFIED		4
#define PR_ST_STOPPED		5
	int state;
	int error;
	struct poller_data data;
	/* In callback, spaces of six pointers are available from here. */
};

```


