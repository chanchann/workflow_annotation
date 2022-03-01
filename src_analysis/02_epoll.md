# workflow 源码解析 : epoll 

项目源码 : https://github.com/sogou/workflow

更加详细的源码注释可看 : https://github.com/chanchann/workflow_annotation

workflow作为一个网络库，必然先从epoll看起

## epoll切入口 ： __poller_wait

从man page我们可以看到

https://man7.org/linux/man-pages/man7/epoll.7.html

epoll一共就三个核心api

epoll_create， epoll_ctl， epoll_wait

而把握了epoll_wait，就能知道怎么处理事件的逻辑。

只有一处出现，这里就是最为核心的切入口

```cpp
/src/kernel/poller.c
static inline int __poller_wait(__poller_event_t *events, int maxevents,
								poller_t *poller)
{
	// int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);
	// Specifying a timeout of -1 causes epoll_wait() to block indefinitely
	return epoll_wait(poller->pfd, events, maxevents, -1);
}
```

此处`__poller_wait` 只是为了跨平台统一接口

## __poller_thread_routine

而调用`__poller_wait`的是`__poller_thread_routine`

```cpp
static void *__poller_thread_routine(void *arg)
{
	...
	while (1)
	{
        ...
		nevents = __poller_wait(events, POLLER_EVENTS_MAX, poller);
        ...
		for (i = 0; i < nevents; i++)
		{
			node = (struct __poller_node *)__poller_event_data(&events[i]);
			if (node > (struct __poller_node *)1)
			{
				switch (node->data.operation)
				{
				case PD_OP_READ:
					__poller_handle_read(node, poller);
					break;
				case PD_OP_WRITE:
					__poller_handle_write(node, poller);
					break;
					...
				}
			}
            ...
		}   
        ...
}
```

这里是将epoll触发的事件数组，挨着挨着根据他们的operation分发给不同的行为函数(read/write....)

## poller_start

`__poller_thread_routine` 是一个线程 `pthread_create` 时输入的执行函数

```cpp
int poller_start(poller_t *poller)
{
	...
	pthread_mutex_lock(&poller->mutex);
	if (__poller_open_pipe(poller) >= 0)
	{
		ret = pthread_create(&tid, NULL, __poller_thread_routine, poller);
		...
	}
	pthread_mutex_unlock(&poller->mutex);
	return -poller->stopped;
}
```

poller_start的作用就是启动 `__poller_thread_routine` 线程。

## mpoller_start

调用 `poller_start` 的是 `mpoller_start`

```cpp
int mpoller_start(mpoller_t *mpoller)
{
	...
	for (i = 0; i < mpoller->nthreads; i++)
	{
		if (poller_start(mpoller->poller[i]) < 0)
			break;
	}
	...;
}
```

mpoller的职责，是start我们设置的epoll线程数的epoll线程

## create_poller

`mpoller_start` 在 `create_poller` 的时候启动

```cpp
int Communicator::create_poller(size_t poller_threads)
{
	struct poller_params params = 默认参数;

	msgqueue_create();  // 创建msgqueue
	mpoller_create(&params, poller_threads);  // 创建poller
	mpoller_start();  // poller 启动

	return -1;
}
```

## Communicator::init

```cpp
int Communicator::init(size_t poller_threads, size_t handler_threads)
{
	....
	create_poller(poller_threads);   // 创建poller线程
	create_handler_threads(handler_threads);  // 创建线程池
	....
}
```

## CommScheduler

```cpp
class CommScheduler
{
public:
	int init(size_t poller_threads, size_t handler_threads)
	{
		return this->comm.init(poller_threads, handler_threads);
	}

    ...
private:
	Communicator comm;
};
```

CommScheduler仅有一个成员变量Communicator, 对于Communicator来说就是对外封装了一层, 加入了一些逻辑操作，本质上都是comm的操作

而`CommScheduler` init 也就是 `Communicator` init

我们也可以看出，参数正好传入了epoll线程数量

## WFGlobal::get_scheduler()

CommScheduler是全局的单例

```cpp
class WFGlobal
{
public:
	static CommScheduler *get_scheduler();
    ...
}
```

CommScheduler作为全局单例就很自然，调度就是全局的策略，而且我们看到了CommScheduler需要创建epoll线程池，不可能在运行我们不断多次创建。

我们知道，单例是第一次调用的时候，创建出对象，那么我们从客户端和服务端两方面来看，什么时候调用`get_scheduler`

## 客户端

我们还是从之前文章写过的TimerTask看

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

在`create_timer_task`调用 `WFGlobal::get_scheduler()`

## 服务端

之前的文章也自此分析过`server`端的代码，其实和酷护短一样，在创建task的时候调用

```cpp
template<class REQ, class RESP>
WFNetworkTask<REQ, RESP> *
WFNetworkTaskFactory<REQ, RESP>::create_server_task(CommService *service,
				std::function<void (WFNetworkTask<REQ, RESP> *)>& process)
{
	return new WFServerTask<REQ, RESP>(service, WFGlobal::get_scheduler(),
									   process);
}
```

## 正向分析流程

我们上文都是从里到外，一层层分析

我们从正常流程出发来分析就清楚了

1. 就拿我们的server来说，我们第一次`create_server_task` 来创建 `server_task` 的时候，调用了全局的 `WFGlobal::get_scheduler()`

2. 初始化CommScheduler

```cpp
CommScheduler *WFGlobal::get_scheduler()
{
	return __CommManager::get_instance()->get_scheduler();
}
```

```cpp
static __CommManager *get_instance()
{
    static __CommManager kInstance;
    return &kInstance;
}
```

```cpp
__CommManager():
    io_server_(NULL),
    io_flag_(false),
    dns_manager_(NULL),
    dns_flag_(false)
{
    const auto *settings = __WFGlobal::get_instance()->get_global_settings();
    if (scheduler_.init(settings->poller_threads,
                        settings->handler_threads) < 0)
        abort();

    signal(SIGPIPE, SIG_IGN);
}
```

这里我们从setting中找到我们设置的epoll线程数，初始化 `CommScheduler`

3. Communicator::init

```cpp
int init(size_t poller_threads, size_t handler_threads)
{
    return this->comm.init(poller_threads, handler_threads);
}
```

4. Communicator::init其实就是在创建并启动epoll线程

```cpp
int Communicator::init(size_t poller_threads, size_t handler_threads)
{
    ...
    this->create_poller(poller_threads);
    ...
}
```

然后这里 `mpoller_create` 为批量生产epoll线程并启动

注意我们的`Communicator::init` 中的 `create_poller` 其实上是 `create_and_start_poller`


5. 创建epoll线程

```cpp
int Communicator::create_poller(size_t poller_threads)
{
    ...
    mpoller_create(&params, poller_threads);
    ...
    mpoller_start(this->mpoller);
    ...
}
```

先创建epoll

```cpp
mpoller_t *mpoller_create(const struct poller_params *params, size_t nthreads)
{
    ...
    __mpoller_create(params, mpoller) >= 0
    ...
}
```

```cpp
static int __mpoller_create(const struct poller_params *params,
							mpoller_t *mpoller)
{
    ...
	for (i = 0; i < mpoller->nthreads; i++)
	{
		mpoller->poller[i] = poller_create(params);  
        ...
	}   
    ...
}
```

```cpp
poller_t *poller_create(const struct poller_params *params)
{
    poller->pfd = __poller_create_pfd();  
    ...
    // 初始化各个poller的参数
    ...
}
```

这里就到底了

```cpp
static inline int __poller_create_pfd()
{
	return epoll_create(1);
}
```

看，这里就是epoll三大api的`epoll_create`, 所以说只要从`epoll_wait`开始，就可以摸清楚epoll如何产生，运行的核心逻辑。

6. 启动epoll线程

```cpp
int mpoller_start(mpoller_t *mpoller)
{
	for (i = 0; i < mpoller->nthreads; i++)
	{
		poller_start(mpoller->poller[i]);
        ...
	}
    ...
}
```

```cpp
int poller_start(poller_t *poller)
{
    ...
    pthread_create(&tid, NULL, __poller_thread_routine, poller);
    ...
}
```

启动了以 `__poller_thread_routine` 为核心的epoll线程

7. epoll线程核心逻辑

```cpp
static void *__poller_thread_routine(void *arg)
{
	...
	while (1)
	{
        ...
		nevents = __poller_wait(events, POLLER_EVENTS_MAX, poller);
        ...
		for (i = 0; i < nevents; i++)
		{
			node = (struct __poller_node *)__poller_event_data(&events[i]);
			if (node > (struct __poller_node *)1)
			{
				switch (node->data.operation)
				{
				case PD_OP_READ:
					__poller_handle_read(node, poller);
					break;
				case PD_OP_WRITE:
					__poller_handle_write(node, poller);
					break;
					...
				}
			}
            ...
		}   
        ...
}
```

这里就是个无限循环，不断的检测是否有事件触发，然后根据注册的operation来分发给不同的操作函数(read/write...)

## 后续

从这里我们就大概理清楚了workflow的核心epoll线程，是如何创建，核心运行的逻辑是什么。

我们这里只是简单分析了整个流程的骨架，但还有很多细节还没分析，比如 `poller_t`，`__poller_node`，`poller_data` 是什么，为何这样设计。我们先屏蔽这些细节，等我们具体分析一个完整的流程再来看隐藏在细节中的魔鬼。