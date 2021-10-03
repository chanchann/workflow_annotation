#! https://zhuanlan.zhihu.com/p/416183766
# workflow 源码解析 05 : ThreadPool00 introduction

更加详细的源码注释可看 : https://github.com/chanchann/workflow_annotation

## 先看接口

接口非常简洁

```cpp
thrdpool_t *thrdpool_create(size_t nthreads, size_t stacksize);
int thrdpool_schedule(const struct thrdpool_task *task, thrdpool_t *pool);
int thrdpool_increase(thrdpool_t *pool);
int thrdpool_in_pool(thrdpool_t *pool);
void thrdpool_destroy(void (*pending)(const struct thrdpool_task *),
					  thrdpool_t *pool);
```

就是线程池创建，销毁，扩容，判断是否在本线程的pool，还有调度

## 线程池的创建(创建消费者)

首先我们创建得知道，线程池的结构

而我们后面就会知道，我们这个结构pthread_setspecific设置成一个thread_local，每个线程都会有一个，所以有tid这些成员

```cpp
typedef struct __thrdpool thrdpool_t;

struct __thrdpool
{
	struct list_head task_queue;
	size_t nthreads;
	size_t stacksize;
	pthread_t tid;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	pthread_key_t key;
	pthread_cond_t *terminate;
};
```

我们看如何创建

```cpp
thrdpool_t *thrdpool_create(size_t nthreads, size_t stacksize)
	// 1. 分配空间
    pool = (thrdpool_t *)malloc(sizeof (thrdpool_t));

    // 2. 初始化
	__thrdpool_init_locks(pool);
	pthread_key_create(&pool->key, NULL)
    INIT_LIST_HEAD(&pool->task_queue);
    pool->stacksize = stacksize;
    pool->nthreads = 0;
    memset(&pool->tid, 0, sizeof (pthread_t));
    pool->terminate = NULL;

    // 3. 创建线程
	__thrdpool_create_threads(nthreads, pool);

	return NULL;
}
```

```cpp
static int __thrdpool_create_threads(size_t nthreads, thrdpool_t *pool)
{
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, pool->stacksize);

		while (pool->nthreads < nthreads)
		{
			pthread_create(&tid, &attr, __thrdpool_routine, pool);
            ...
		}
}

```

然后可见我们线程跑的是__thrdpool_routine

```cpp
static void *__thrdpool_routine(void *arg)
{
	thrdpool_t *pool = (thrdpool_t *)arg;
	struct list_head **pos = &pool->task_queue.next;  
	pthread_setspecific(pool->key, pool);

	while (1)
	{
		pthread_mutex_lock(&pool->mutex);

		while (!pool->terminate && list_empty(&pool->task_queue))
			pthread_cond_wait(&pool->cond, &pool->mutex);

		entry = list_entry(*pos, struct __thrdpool_task_entry, list);
		list_del(*pos);

		pthread_mutex_unlock(&pool->mutex);

		task_routine = entry->task.routine;
		task_context = entry->task.context;
		free(entry);
		task_routine(task_context);
        
        ... 
	}   
    ...
}
```

线程池就是开启了许多消费者线程，如果有任务，就拿出来运行，否则就wait

## thrdpool_schedule，生产者PUT task

我们上次有个地方说到，我们这里就是把task封装好，交给线程池处理。

```cpp
/src/kernel/Executor.cc

int Executor::request(ExecSession *session, ExecQueue *queue)
{
    ... 
	session->queue = queue;
	entry = (struct ExecTaskEntry *)malloc(sizeof (struct ExecTaskEntry));

    entry->session = session;
    entry->thrdpool = this->thrdpool;

    pthread_mutex_lock(&queue->mutex);
    list_add_tail(&entry->list, &queue->task_list); 

    if (queue->task_list.next == &entry->list)
    {
        struct thrdpool_task task = {
            .routine	=	Executor::executor_thread_routine,
            .context	=	queue
        };
        thrdpool_schedule(&task, this->thrdpool);
    }

    pthread_mutex_unlock(&queue->mutex);
    ...
}

```

这里把task(executor_thread_routine)交给线程池处理

```cpp
int thrdpool_schedule(const struct thrdpool_task *task, thrdpool_t *pool)
{
	void *buf = malloc(sizeof (struct __thrdpool_task_entry));
    __thrdpool_schedule(task, buf, pool);

}
```

```cpp
inline void __thrdpool_schedule(const struct thrdpool_task *task, void *buf,
								thrdpool_t *pool)
{
	struct __thrdpool_task_entry *entry = (struct __thrdpool_task_entry *)buf;

	entry->task = *task;
	pthread_mutex_lock(&pool->mutex);
	
    list_add_tail(&entry->list, &pool->task_queue);
	pthread_cond_signal(&pool->cond);

	pthread_mutex_unlock(&pool->mutex);
}
```

这里就是添加至队列就完了，生产者消费者模型，这里就是生产者put，通知消费者来消费

## 未完待续

以上是线程池最核心的两个接口，因为代表着线程池生产者-消费者这个本质的模型。

我在创建的时候，就产生一堆消费者在这wait等待任务来，而我们的thrdpool_schedule就是put任务进来，是个生产者

线程池最为复杂的是各个情况的优雅退出，需要再多多仔细分析，写写demo。

还有就是扩容线程池。

还有仔细分析这里的pthread_key_t 的作用。