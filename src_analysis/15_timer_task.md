#! https://zhuanlan.zhihu.com/p/416865725
# workflow 源码解析 07 : TimerTask

项目源码 : https://github.com/sogou/workflow

更加详细的源码注释可看 : https://github.com/chanchann/workflow_annotation

看Timer task，可以了解下怎么实现一个异步的定时器，这个时候就开始接触Communicator和Session了

## 先写一个小demo

```cpp
int main()
{
    WFFacilities::WaitGroup wait_group(1);
    auto start = std::chrono::high_resolution_clock::now();
    WFTimerTask *task = WFTaskFactory::create_timer_task(10 * 1000,    
    [&start, &wait_group](WFTimerTask *timer)
    {
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> tm = end - start;
        spdlog::info("time consume : {}", tm.count());
        wait_group.done();
    });
    task->start();
    wait_group.wait();
    return 0;
}
```

## 创建流程


### create_timer_task

```cpp
WFTimerTask *WFTaskFactory::create_timer_task(unsigned int microseconds,
											  timer_callback_t callback)
{
	return new __WFTimerTask((time_t)(microseconds / 1000000),
							 (long)(microseconds % 1000000 * 1000),
							 WFGlobal::get_scheduler(),
							 std::move(callback));
}
```

### __WFTimerTask

实际上是new了一个__WFTimerTask

并且在构造的时候，`WFGlobal::get_scheduler()` 创建了 CommScheduler

```cpp
class __WFTimerTask : public WFTimerTask
{
protected:
	virtual int duration(struct timespec *value)
	{
		value->tv_sec = this->seconds;
		value->tv_nsec = this->nanoseconds;
		return 0;
	}

protected:
	time_t seconds;
	long nanoseconds;

public:
	__WFTimerTask(time_t seconds, long nanoseconds, CommScheduler *scheduler,
				  timer_callback_t&& cb) :
		WFTimerTask(scheduler, std::move(cb))
	{
		this->seconds = seconds;
		this->nanoseconds = nanoseconds;
	}
};
```
### WFTimerTask

继承的WFTimerTask 是个中规中矩的一个task

```cpp

class WFTimerTask : public SleepRequest
{
public:
	void start();
	void dismiss();
	void *user_data;
	int get_state() const { return this->state; }
	int get_error() const { return this->error; }
	void set_callback(std::function<void (WFTimerTask *)> cb);

protected:
	virtual SubTask *done();
	std::function<void (WFTimerTask *)> callback;

public:
	WFTimerTask(CommScheduler *scheduler,
				std::function<void (WFTimerTask *)> cb) :
		SleepRequest(scheduler),
		callback(std::move(cb))
	{
        ...
	}

};

```

### 总结结构

再回看下threadTask的结构，就大概知道wf的继承架构。

一个中规中矩的task为轴，向下__xxxTask添加独特功能，向上继承自kernel中相关的xxxRequest, 而xxxRequest又满足subTask又满足相关xxxSession性质从而继承。

xxxSession中有两个纯虚函数，一个handle让xxxRequest来实现，一个要一直到__WFxxxTask才实现，这样__WFxxxxTask才实现完所有的纯虚函数，才能实例化，所有的接口都是session *， 从而满足多态.

核心入口就是 xxxRequest 要实现dispatch 去的调用 scheduler(threadTask 是executor) 完成相关功能。

### SleepRequest

```cpp
class SleepRequest : public SubTask, public SleepSession
{
public:
	SleepRequest(CommScheduler *scheduler)
	{
		this->scheduler = scheduler;
	}

public:
	virtual void dispatch()
	{
		if (this->scheduler->sleep(this) < 0)
		{
			this->state = SS_STATE_ERROR;
			this->error = errno;
			this->subtask_done();
		}
	}

protected:
	int state;
	int error;

protected:
	CommScheduler *scheduler;

protected:
	virtual void handle(int state, int error)
	{
		this->state = state;
		this->error = error;
		this->subtask_done();
	}
};
```

### SleepSession

我们窥探下SleepSession，就是两个纯虚函数给我们去实现。

```cpp
class SleepSession
{
private:
	virtual int duration(struct timespec *value) = 0;
	virtual void handle(int state, int error) = 0;
    ...
};
```

### CommScheduler

回到我们说sleep的核心是`this->scheduler->sleep(this)`

```cpp
class CommScheduler
{
public:
	int init(size_t poller_threads, size_t handler_threads)
	{
		return this->comm.init(poller_threads, handler_threads);
	}
	/* for sleepers. */
	int sleep(SleepSession *session)
	{
		return this->comm.sleep(session);
	}
    ...
private:
	Communicator comm;
};
```

实际上调用

```cpp
int Communicator::sleep(SleepSession *session)
{
	struct timespec value;

	if (session->duration(&value) >= 0)
	{
		if (mpoller_add_timer(&value, session, this->mpoller) >= 0)
			return 0;
	}

	return -1;
}
```

然后就是向poller上添加定时器

```cpp
static inline int mpoller_add_timer(const struct timespec *value, void *context,
									mpoller_t *mpoller)
{
	static unsigned int n = 0;
	unsigned int index = n++ % mpoller->nthreads;
	return poller_add_timer(value, context, mpoller->poller[index]);
}
```

就是round robin 随机找个poller线程添加上

```cpp

int poller_add_timer(const struct timespec *value, void *context,
					 poller_t *poller)
{
	struct __poller_node *node;

	node = (struct __poller_node *)malloc(sizeof (struct __poller_node));
	if (node)
	{
		memset(&node->data, 0, sizeof (struct poller_data));
		node->data.operation = PD_OP_TIMER;
		node->data.fd = -1;
		node->data.context = context;
		node->in_rbtree = 0;
		node->removed = 0;
		node->res = NULL;

		clock_gettime(CLOCK_MONOTONIC, &node->timeout);
		node->timeout.tv_sec += value->tv_sec;
		node->timeout.tv_nsec += value->tv_nsec;
		if (node->timeout.tv_nsec >= 1000000000)
		{
			node->timeout.tv_nsec -= 1000000000;
			node->timeout.tv_sec++;
		}

		pthread_mutex_lock(&poller->mutex);
		__poller_insert_node(node, poller);
		pthread_mutex_unlock(&poller->mutex);
		return 0;
	}

	return -1;
}
```

```cpp
int poller_add_timer(const struct timespec *value, void *context,
					 poller_t *poller)
{
	LOG_TRACE("poller_add_timer");
	struct __poller_node *node;

	node = (struct __poller_node *)malloc(sizeof (struct __poller_node));
	if (node)
	{
		memset(&node->data, 0, sizeof (struct poller_data));
		node->data.operation = PD_OP_TIMER;
		node->data.fd = -1;
		node->data.context = context;
		node->in_rbtree = 0;
		node->removed = 0;
		node->res = NULL;

		clock_gettime(CLOCK_MONOTONIC, &node->timeout);
		node->timeout.tv_sec += value->tv_sec;
		node->timeout.tv_nsec += value->tv_nsec;
		if (node->timeout.tv_nsec >= 1000000000)
		{
			node->timeout.tv_nsec -= 1000000000;
			node->timeout.tv_sec++;
		}

		pthread_mutex_lock(&poller->mutex);
		__poller_insert_node(node, poller); 
		pthread_mutex_unlock(&poller->mutex);
		return 0;
	}

	return -1;
}
```

实例化一个node并初始化后，然后插入poller进行关注。

当定时完成，触发epoll_wait返回事件，继续完成。