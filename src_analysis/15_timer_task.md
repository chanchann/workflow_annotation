#! https://zhuanlan.zhihu.com/p/416865725
# workflow 源码解析 : TimerTask

项目源码 : https://github.com/sogou/workflow

更加详细的源码注释可看 : https://github.com/chanchann/workflow_annotation

看Timer task，可以了解下怎么实现一个异步的定时器，这个时候就开始接触Communicator和Session了

## 先写一个小demo

```cpp
#include <chrono>
#include <workflow/Workflow.h>
#include <workflow/WFTaskFactory.h>
#include <workflow/WFFacilities.h>

int main()
{
    WFFacilities::WaitGroup wait_group(1);
    auto start = std::chrono::high_resolution_clock::now();
    WFTimerTask *task = WFTaskFactory::create_timer_task(10 * 1000,    
    [&start, &wait_group](WFTimerTask *timer)
    {
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> tm = end - start;
        fprintf(stderr, "time consume : %f ms\n", tm.count());
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

此处有两个核心要点

1. `__WFTimerTask`

2. WFGlobal::get_scheduler()

### __WFTimerTask

对比下，`__WFThreadTask`, `__WFGoTask`, 我们已经找到了一点task继承的规律

这里的 `__WFTimerTask`， 最为核心的是就是时间间隔成员，还有duration这个核心函数

基于之前的经验，我盲猜，`duration`, `handle`会是两个纯虚函数让我们实现。其父类必然也是个中规中矩的task

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

果然，继承的WFTimerTask 是个中规中矩的一个task

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

## 总结结构

对比回看下 `ThreadTask` 的结构，就大概知道wf的继承架构

### 1. 一个中规中矩的 `xxxTask` 为轴

### 2. 向下 `__xxxTask` 添加独特功能

比如 `__ThreadTask` 添加 `routine`，增加 `execute`.

`__GoTask` 添加 `callback(go)`, 增加 `execute`

`__WFTimerTask` 添加 `seconds`, `nanoseconds`, 增加 `duration`.

### 3. 向上继承自kernel中相关的 `xxxRequest`

而 `xxxRequest` 又满足 `subTask` 又满足相关 `xxxSession`性质从而继承。

其中继承自 `subTask` 十分自然，因为都是task，`subTask` 是所有task的祖先，而 `xxxSession` 代表task相关的属性

比如 `__ThreadTask` 和 `__GoTask` 都是计算型任务，计算任务还具有 `ExecSession` 的特性

`__WFTimerTask` 是休眠任务，具有 `SleepSession` 的特性，这个我们下面分析

而网络相关的task则是具有 `CommSession` 的特性, CommSession是一次req->resp的交互

### 4. `xxxSession` 中有两个纯虚函数

一个 `handle` 让 `xxxRequest` 来实现，另一个要一直到 `__xxxTask`才实现，这样`__xxxxTask`才实现完所有的纯虚函数，才能实例化，所有的接口都是`session *`， 从而满足多态

这个对比来看，其中 `handle` 都一样，主要是不同任务特有的接口，比如，计算任务是 `execute`, 休眠任务是 `duration`, 网络任务则是 `message_in`, `message_out` 

(网络任务更为复杂，细节我们可以等分析http看，不过大抵也是如此)

### 5. 核心入口就是 `xxxRequest` 要实现 `dispatch` 

去的调用 scheduler(threadTask 是executor) 完成相关功能

### SleepRequest

```cpp
class SleepRequest : public SubTask, public SleepSession
{
public:
	SleepRequest(CommScheduler *scheduler);

	virtual void dispatch()
	{
		...
		this->scheduler->sleep(this);
		...
	}

protected:
	int state;
	int error;
	CommScheduler *scheduler;
	virtual void handle(int state, int error);
};
```

我们根据之前的总结，`SleepRequest` 核心入口 `dispatch`, 整个task的核心就在此 `scheduler->sleep`

### SleepSession

我们窥探下SleepSession，就是这两个纯虚函数给我们去实现。

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

回到了我们所实现的接口`duration`, `session->duration`, 而且根据我们总结的，以`session`形式，利用多态。

而我们的 `duration` 只是设置时间数值，我们这里的休眠操作是异步的，肯定是要往epoll上面挂

所以我们这里就是向poller上添加定时器

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
	...
	memset(&node->data, 0, sizeof (struct poller_data));
	node->data.operation = PD_OP_TIMER;
	node->data.fd = -1;
	node->data.context = context;   // 是session
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
	...
}
```

实例化一个node并初始化后，然后插入poller进行关注。

当定时完成，触发epoll_wait返回事件，继续完成。