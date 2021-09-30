## workflow 源码解析 05 : ThreadPool

## 线程池的创建

## 生产者-消费者

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

这里的thrdpool_task 就是两个成员，一个回调，一个参数

```cpp
struct thrdpool_task
{
	void (*routine)(void *);
	void *context;
};
```

然后交给线程池处理

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

