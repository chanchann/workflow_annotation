# workflow源码解析 : epoll part1

项目源码 : https://github.com/sogou/workflow

更加详细的源码注释可看 : https://github.com/chanchann/workflow_annotation

workflow作为一个网络库，必然先从epoll看起，把握了epoll_wait，就能知道怎么处理事件的逻辑。

```
// perl calltree.pl "epoll_wait" "" 1 1 3
// perl calltree.pl "poller_start" "" 1 1 3
// 两个的结果拼凑一起
epoll_wait
└── __poller_wait     [vim src/kernel/poller.c +155]
    └── __poller_thread_routine       [vim src/kernel/poller.c +963]
				└── poller_start
					└── mpoller_start     [vim src/kernel/mpoller.c +85]
						└── Communicator::create_poller   [vim src/kernel/Communicator.cc +1319]
							└── Communicator::init        [vim src/kernel/Communicator.cc +1356]
```

此处__poller_wait 只是为了跨平台统一接口

所以真正管理事件的是__poller_thread_routine

```cpp
static void *__poller_thread_routine(void *arg)
{
	...
	while (1)
	{
		__poller_set_timer(poller);
		nevents = __poller_wait(events, POLLER_EVENTS_MAX, poller);
		clock_gettime(CLOCK_MONOTONIC, &time_node.timeout);
		has_pipe_event = 0;
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
			else if (node == (struct __poller_node *)1)
				has_pipe_event = 1;
		}

		if (has_pipe_event)
		{
			if (__poller_handle_pipe(poller))
				break;
		}

		__poller_handle_timeout(&time_node, poller);
	}
	return NULL;
}
```

而他是在poller_start中开启一个线程


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

而调用poller_start的是mpoller_start，根据mpoller的职责，是批量start我们设置的一堆epoll线程

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

而mpoller_start 在create_poller的时候启动

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

而他是init的时候创建poller

```cpp
int Communicator::init(size_t poller_threads, size_t handler_threads)
{
	....
	create_poller(poller_threads);   // 创建poller线程
	create_handler_threads(handler_threads);  // 创建线程池
	....
}
```

而Communicator::init就由外部这些任务去产生了。

```
── Communicator::init    [vim src/kernel/Communicator.cc +1356]
          ├── ParsedURI [vim src/util/URIParser.h +51]
          ├── WFServerBase::init        [vim src/server/WFServer.cc +88]
          ├── WFServerBase::start       [vim src/server/WFServer.cc +166]
          ├── MySQLResultCursor::reset  [vim src/protocol/MySQLResult.cc +84]
          ├── Communicator::accept      [vim src/kernel/Communicator.cc +1262]
          ├── CommSchedTarget::init     [vim src/kernel/CommScheduler.cc +47]
          ├── WFTaskFactory::create_http_task   [vim src/factory/HttpTaskImpl.cc +779]
          ├── WFTaskFactory::create_mysql_task  [vim src/factory/MySQLTaskImpl.cc +679]
          ├── WFTaskFactory::create_dns_task    [vim src/factory/DnsTaskImpl.cc +194]
          ├── WFTaskFactory::create_redis_task  [vim src/factory/RedisTaskImpl.cc +265]
```

我们还是取最熟悉的http来分析, 用bt来查看调用情况

```
cgdb ./http_req

(gdb) b Communicator.cc:Communicator::init
(gdb) bt

#0  Communicator::init (
    this=this@entry=0x555555858448 <__CommManager::get_instance()::kInstance+8>, poller_threads=4, handler_threads=20)
    at /home/ysy/workflow/src/kernel/Communicator.cc:1343
#1  0x00005555555dbff7 in CommScheduler::init (
    handler_threads=<optimized out>, poller_threads=<optimized out>, 
    this=0x555555858440 <__CommManager::get_instance()::kInstance>)
    at /home/ysy/workflow/_include/workflow/CommScheduler.h:116
#2  __CommManager::__CommManager (
    this=0x555555858440 <__CommManager::get_instance()::kInstance>)
    at /home/ysy/workflow/src/manager/WFGlobal.cc:387
#3  __CommManager::get_instance ()
    at /home/ysy/workflow/src/manager/WFGlobal.cc:339
#4  WFGlobal::get_scheduler ()
    at /home/ysy/workflow/src/manager/WFGlobal.cc:720
#5  0x00005555555e3714 in WFComplexClientTask<protocol::HttpRequest, protocol::HttpResponse, bool>::WFComplexClientTask(int, std::function<void (WFNetworkTask<protocol::HttpRequest, protocol::HttpResponse>*)>&&) (cb=..., retry_max=2, 
    this=0x55555586b030)
    at /home/ysy/workflow/src/factory/WFTaskFactory.inl:104
#6  ComplexHttpTask::ComplexHttpTask(int, int, std::function<void (WFNetworkTask<protocol::HttpRequest, protocol::HttpResponse>*)>&&) (callback=..., 
    retry_max=2, redirect_max=4, this=0x55555586b030)
    at /home/ysy/workflow/src/factory/HttpTaskImpl.cc:51
#7  WFTaskFactory::create_http_task(std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&, int, int, std::function<void (WFNetworkTask<protocol::HttpRequest, protocol::HttpResponse>*)>) (
    url="http://www.baidu.com", redirect_max=4, retry_max=2, callback=...)
    at /home/ysy/workflow/src/factory/HttpTaskImpl.cc:755
#8  0x00005555555bba0e in main ()
    at /home/ysy/workflow_annotation/demos/07_http/http_req.cc:49
```

我们从栈底分析，先从main函数中create_http_task

```cpp
WFHttpTask *WFTaskFactory::create_http_task(const ParsedURI& uri,
											int redirect_max,
											int retry_max,
											http_callback_t callback)
{
	auto *task = new ComplexHttpTask();
	task->init();  
}
```

在里面实际上是new了一个ComplexHttpTask

在构造ComplexHttpTask的父类WFComplexClientTask时

```cpp
WFComplexClientTask(int retry_max, task_callback_t&& cb):
	WFClientTask<REQ, RESP>(NULL, WFGlobal::get_scheduler(), std::move(cb))
{
	... 一堆设置
}
```

注意我们在这里传入了 WFGlobal::get_scheduler()

```cpp
CommScheduler *WFGlobal::get_scheduler()
{
	return __CommManager::get_instance()->get_scheduler();
}
```

__CommManager 是一个单例

在构造的时候调用了CommScheduler的init

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

从而调用了Communicator的comm.

## In conclusion

我们在create_http_task的时候，构造出task时，就init Communicator, 从而创建poller线程，创建线程池。

每个poller线程启动就是开一个执行 __poller_thread_routine 函数的线程，在每个poller线程中执行事件的监听。


## 如何防止重复创建

我们需要创建很多个http task 串起来执行，如何只用一套poller线程和线程池呢

原因在于__CommManager 是一个单例， 只init一次(只创建一次线程池)


