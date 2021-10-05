#! https://zhuanlan.zhihu.com/p/415833220
# workflow 源码解析 04 : ThreadTask

更加详细的源码注释可看 : https://github.com/chanchann/workflow_annotation

我们先读源码可以先从thread task入手，相对而言比较容易理解（因为不涉及网络相关的内容）

最主要的是了解workflow中“任务”到底是什么（透露一下，在SubTask.h中定义 

这部分主要涉及kernel中的线程池和队列


## 先写一个简单的加法运算程序 

https://github.com/chanchann/workflow_annotation/blob/main/demos/24_thrd_task/24_thrd_task_01.cc

```cpp
#include <iostream>
#include <workflow/WFTaskFactory.h>
#include <workflow/WFFacilities.h>
#include <spdlog/spdlog.h>
#include <signal.h>
#include <errno.h>

// 直接定义thread_task三要素

// 定义INPUT
struct AddInput
{
    int x;
    int y;
};

// 定义OUTPUT
struct AddOutput
{
    int res;
};

// 加法流程
void add_routine(const AddInput *input, AddOutput *output)
{
    output->res = input->x + input->y;
}

using AddTask = WFThreadTask<AddInput, AddOutput>;

void callback(AddTask *task)
{
	auto *input = task->get_input();
	auto *output = task->get_output();

	assert(task->get_state() == WFT_STATE_SUCCESS);
    spdlog::info("{} + {} = {}", input->x, input->y, output->res);
}

int main()
{
    using AddFactory = WFThreadTaskFactory<AddInput, AddOutput>;
	AddTask *task = AddFactory::create_thread_task("add_task",
												add_routine,
												callback);
	auto *input = task->get_input();

	input->x = 1;
	input->y = 2;

	WFFacilities::WaitGroup wait_group(1);

	Workflow::start_series_work(task, [&wait_group](const SeriesWork *) {
		wait_group.done();
	});

	wait_group.wait();
	return 0;
}
```

## thread_task 的使用

首先我们创建一个thread_task 

```cpp
using AddFactory = WFThreadTaskFactory<AddInput, AddOutput>;
AddTask *task = AddFactory::create_thread_task("add_task",
                                            add_routine,
                                            callback);
```

## thread_task 结构

观察下结构，首先看看工厂

### WFThreadTaskFactory

```cpp
template<class INPUT, class OUTPUT>
class WFThreadTaskFactory
{
private:
	using T = WFThreadTask<INPUT, OUTPUT>;
    ...
public:
	static T *create_thread_task(const std::string& queue_name,
								 std::function<void (INPUT *, OUTPUT *)> routine,
								 std::function<void (T *)> callback);

    ...
};
```

需要的就三个东西INPUT，OUTPUT，和routine。

INPUT和OUTPUT是两个模板参数，可以是任何类型。routine表示从INPUT到OUTPUT的过程

```cpp
template<class INPUT, class OUTPUT>
WFThreadTask<INPUT, OUTPUT> *
WFThreadTaskFactory<INPUT, OUTPUT>::create_thread_task(const std::string& queue_name,
						std::function<void (INPUT *, OUTPUT *)> routine,
						std::function<void (WFThreadTask<INPUT, OUTPUT> *)> callback)
{
	return new __WFThreadTask<INPUT, OUTPUT>(WFGlobal::get_exec_queue(queue_name),
											 WFGlobal::get_compute_executor(),
											 std::move(routine),
											 std::move(callback));
}
```

这里可以看到，其实就是 new 一个 __WFThreadTask

在此处，创建了ExecQueue 和 Executor，而且由于`WFGlobal::`, 所以由单例`__ExecManager`来管控

下面会详细讲述

### __WFThreadTask

其中最为核心的就在两点，一个是继承自WFThreadTask，一个就是最为核心的成员变量routine。

```cpp
template<class INPUT, class OUTPUT>
class __WFThreadTask : public WFThreadTask<INPUT, OUTPUT>
{
protected:
	virtual void execute()
	{
		this->routine(&this->input, &this->output);
	}

protected:
	std::function<void (INPUT *, OUTPUT *)> routine;

public:
	__WFThreadTask(ExecQueue *queue, Executor *executor,
				   std::function<void (INPUT *, OUTPUT *)>&& rt,
				   std::function<void (WFThreadTask<INPUT, OUTPUT> *)>&& cb) :
		WFThreadTask<INPUT, OUTPUT>(queue, executor, std::move(cb)),
		routine(std::move(rt))
	{
	}
};
```

### WFThreadTask

而他所继承的WFThreadTask就很中规中矩了，这里的继承相当于是给他加上了routine这个特性功能，并且把这一层给隐藏起来

```cpp
template<class INPUT, class OUTPUT>
class WFThreadTask : public ExecRequest
{
public:
	void start();
	void dismiss();

	INPUT *get_input() { return &this->input; }
	OUTPUT *get_output() { return &this->output; }

	void *user_data;

	int get_state() const { return this->state; }
	int get_error() const { return this->error; }

	void set_callback(std::function<void (WFThreadTask<INPUT, OUTPUT> *)> cb);
protected:
	virtual SubTask *done();

protected:
	INPUT input;
	OUTPUT output;
	std::function<void (WFThreadTask<INPUT, OUTPUT> *)> callback;

public:
	WFThreadTask(ExecQueue *queue, Executor *executor,
				 std::function<void (WFThreadTask<INPUT, OUTPUT> *)>&& cb) :
		ExecRequest(queue, executor),
		callback(std::move(cb))
	{
        // 初始化
	}

protected:
	virtual ~WFThreadTask() { }
};
```

### ExecRequest

这里继承了ExecRequest, 其中最为核心的就是三点

1. 继承自SubTask， ExecSession

Task最终祖先都是SubTask，毋庸置疑，计算任务还具有ExecSession的特性

而网络的task则是继承自SubTask和CommSession, 而CommSession是一次req->resp的交互

2. 在这一层实现了dispatch，核心是完成executor->request

3. 两个重要的成员变量ExecQueue, Executor

```cpp
/src/kernel/ExecRequest.h
class ExecRequest : public SubTask, public ExecSession
{
public:
	ExecRequest(ExecQueue *queue, Executor *executor);
	ExecQueue *get_request_queue() const { return this->queue; }
	void set_request_queue(ExecQueue *queue) { this->queue = queue; }
	virtual void dispatch()
	{
		this->executor->request(this, this->queue);
	}

protected:
	int state;
	int error;

	ExecQueue *queue;
	Executor *executor;

protected:
	virtual void handle(int state, int error);
};

```

## ExecSession

有两个虚函数需要我们去实现

```cpp
/src/kernel/Executor.h
class ExecSession
{
private:
	virtual void execute() = 0;
	virtual void handle(int state, int error) = 0;

protected:
	ExecQueue *get_queue() { return this->queue; }

private:
	ExecQueue *queue;
    ...
};
```

再次看下我们的UML

![pic](https://github.com/chanchann/workflow_annotation/blob/main/src_analysis/pics/exeReuest.png?raw=true)

其中的handle在子类ExecRequest中实现了,

而execute在__WFThreadTask实现，到这里才把纯虚函数实现完，才能实例化，所以我们new的是__WFThreadTask.

### 其中两个重要成员: ExecQueue, Executor

两者都在创建__WFThreadTask的时候创建。

```cpp
template<class INPUT, class OUTPUT>
WFThreadTask<INPUT, OUTPUT> *
WFThreadTaskFactory<INPUT, OUTPUT>::create_thread_task(const std::string& queue_name,
						std::function<void (INPUT *, OUTPUT *)> routine,
						std::function<void (WFThreadTask<INPUT, OUTPUT> *)> callback)
{
	return new __WFThreadTask<INPUT, OUTPUT>(WFGlobal::get_exec_queue(queue_name),
											 WFGlobal::get_compute_executor(),
											 std::move(routine),
											 std::move(callback));
}
```

### ExecQueue

我们先看看ExecQueue的结构

```cpp
/src/kernel/Executor.h
class ExecQueue
{
    ...
private:
	struct list_head task_list;
	pthread_mutex_t mutex;
};
```

顾名思义，就是一个执行队列

我们来看看他的创建过程

```cpp
ExecQueue *get_exec_queue(const std::string& queue_name)
{
	pthread_rwlock_rdlock(&rwlock_);
	const auto iter = queue_map_.find(queue_name);

	if (iter != queue_map_.cend())
		queue = iter->second;

	pthread_rwlock_unlock(&rwlock_);

	if (!queue)
	{
		queue = new ExecQueue();  
		queue->init();

		pthread_rwlock_wrlock(&rwlock_);
		
		const auto ret = queue_map_.emplace(queue_name, queue);s
		...
		pthread_rwlock_unlock(&rwlock_);
}
```

### Executor

我们看看Executor的创建过程

```cpp
/src/kernel/WFGlobal.cc
class __ExecManager
{
	Executor *get_compute_executor() { return &compute_executor_; }

private:
	__ExecManager():
		rwlock_(PTHREAD_RWLOCK_INITIALIZER)
	{
		int compute_threads = __WFGlobal::get_instance()->
										  get_global_settings()->
										  compute_threads;
		compute_executor_.init(compute_threads);
		...
	}

private:
	...
	Executor compute_executor_;
};

```

Executor与__ExecManager是组合关系，是全局唯一，声明周期相同

```cpp
int Executor::init(size_t nthreads)
{
	this->thrdpool = thrdpool_create(nthreads, 0);	
	...
}
```

init知道Executor就是个计算线程池

我们再看看Executor的结构

```cpp
/src/kernel/Executor.h
class Executor
{
public:
    ...
	int request(ExecSession *session, ExecQueue *queue);

private:
	struct __thrdpool *thrdpool;

private:
	static void executor_thread_routine(void *context);
	static void executor_cancel_tasks(const struct thrdpool_task *task);

};
```

而Executor看成员，也知道是个线程池。

其中最为重要的就是request 和 executor_thread_routine

我们用个list就知道，我们的实体需要包裹起来这个list node才能串起来，首先我们看看执行任务实体

```cpp
struct ExecTaskEntry
{
	struct list_head list;
	ExecSession *session;
	thrdpool_t *thrdpool;
};
```

其中的session就是执行execute的实体,__WFThreadTask->execute();

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

主要就是把任务加入队列中等待执行, 如果下一个就是我们这个任务，则交给线程池处理。

线程池是怎么处理运行的细节我们留到下线程池的章节，我们这里知道他把task交给线程池处理就可。

我们在线程池中进行的回调是

```cpp
/src/kernel/Executor.cc
void Executor::executor_thread_routine(void *context)
{
	...
	pthread_mutex_lock(&queue->mutex);

	entry = list_entry(queue->task_list.next, struct ExecTaskEntry, list);
	list_del(&entry->list);
	session = entry->session;
	if (!list_empty(&queue->task_list))
	{
		struct thrdpool_task task = {
			.routine	=	Executor::executor_thread_routine,
			.context	=	queue
		};
		__thrdpool_schedule(&task, entry, entry->thrdpool);
	}
	else
		free(entry);

	pthread_mutex_unlock(&queue->mutex);

	session->execute();
	session->handle(ES_STATE_FINISHED, 0);
}
```


1. 执行此任务

```cpp
session->execute();
session->handle(ES_STATE_FINISHED, 0);
```

todo : thrdpool_schedule 这里需要再整理一番

## 日志流程

```
T 21-10-03 23:41:43.907341 [tid : 10039] <WFTaskFactory.inl:605> : create_thread_task
T 21-10-03 23:41:43.909882 [tid : 10039] <Executor.h:82> : Executor creator
T 21-10-03 23:41:43.912471 [tid : 10039] <WFGlobal.cc:537> : __ExecManager creator
T 21-10-03 23:41:43.913831 [tid : 10039] <Executor.cc:66> : Executor::init
T 21-10-03 23:41:43.914381 [tid : 10039] <Executor.cc:72> : Calculate thread pool create
T 21-10-03 23:41:43.914764 [tid : 10039] <Executor.h:39> : ExecQueue creator
T 21-10-03 23:41:43.914952 [tid : 10039] <Executor.cc:42> : ExecQueue::init
T 21-10-03 23:41:43.915091 [tid : 10039] <Executor.h:61> : ExecSession creator
T 21-10-03 23:41:43.915205 [tid : 10039] <ExecRequest.h:31> : ExecRequest creator
T 21-10-03 23:41:43.915294 [tid : 10039] <WFTask.h:119> : WFThreadTask constructor
T 21-10-03 23:41:43.915473 [tid : 10039] <WFTaskFactory.inl:595> : __WFThreadTask constructor
// 之前可以看出构造的流程

T 21-10-03 23:41:43.916085 [tid : 10039] <ExecRequest.h:42> : ExecRequest dispatch
T 21-10-03 23:41:43.916254 [tid : 10039] <Executor.cc:144> : Executor::request

// 这两句才是调用的核心

T 21-10-03 23:41:43.916388 [tid : 10039] <Executor.cc:158> : add task to task_list
T 21-10-03 23:41:43.916513 [tid : 10039] <thrdpool.c:293> : thrdpool_schedule
T 21-10-03 23:41:43.916679 [tid : 10039] <thrdpool.c:267> : __thrdpool_schedule
T 21-10-03 23:41:43.916811 [tid : 10039] <thrdpool.c:274> : add entry list to pool task Queue
T 21-10-03 23:41:43.917034 [tid : 10039] <WFGlobal.cc:387> : __CommManager creator
T 21-10-03 23:41:43.917166 [tid : 10039] <CommScheduler.h:118> : CommScheduler::init
T 21-10-03 23:41:43.917466 [tid : 10039] <Communicator.cc:1294> : create handler thread pool
T 21-10-03 23:41:43.919664 [tid : 10039] <thrdpool.c:293> : thrdpool_schedule
T 21-10-03 23:41:43.920183 [tid : 10039] <thrdpool.c:267> : __thrdpool_schedule
T 21-10-03 23:41:43.920285 [tid : 10039] <thrdpool.c:274> : add entry list to pool task Queue
T 21-10-03 23:41:43.920300 [tid : 10039] <thrdpool.c:293> : thrdpool_schedule
T 21-10-03 23:41:43.920311 [tid : 10039] <thrdpool.c:267> : __thrdpool_schedule
T 21-10-03 23:41:43.920350 [tid : 10039] <thrdpool.c:274> : add entry list to pool task Queue
T 21-10-03 23:41:43.920362 [tid : 10039] <thrdpool.c:293> : thrdpool_schedule
T 21-10-03 23:41:43.920372 [tid : 10039] <thrdpool.c:267> : __thrdpool_schedule
T 21-10-03 23:41:43.920489 [tid : 10039] <thrdpool.c:274> : add entry list to pool task Queue
T 21-10-03 23:41:43.920501 [tid : 10039] <thrdpool.c:293> : thrdpool_schedule
T 21-10-03 23:41:43.920511 [tid : 10039] <thrdpool.c:267> : __thrdpool_schedule
T 21-10-03 23:41:43.920554 [tid : 10039] <thrdpool.c:274> : add entry list to pool task Queue
T 21-10-03 23:41:43.920573 [tid : 10039] <thrdpool.c:293> : thrdpool_schedule
T 21-10-03 23:41:43.920982 [tid : 10039] <thrdpool.c:267> : __thrdpool_schedule
T 21-10-03 23:41:43.921003 [tid : 10039] <thrdpool.c:274> : add entry list to pool task Queue
T 21-10-03 23:41:43.921035 [tid : 10039] <thrdpool.c:293> : thrdpool_schedule
T 21-10-03 23:41:43.921055 [tid : 10039] <thrdpool.c:267> : __thrdpool_schedule
T 21-10-03 23:41:43.921067 [tid : 10039] <thrdpool.c:274> : add entry list to pool task Queue
T 21-10-03 23:41:43.921087 [tid : 10039] <thrdpool.c:293> : thrdpool_schedule
T 21-10-03 23:41:43.921108 [tid : 10039] <thrdpool.c:267> : __thrdpool_schedule
T 21-10-03 23:41:43.921125 [tid : 10039] <thrdpool.c:274> : add entry list to pool task Queue
T 21-10-03 23:41:43.921223 [tid : 10039] <thrdpool.c:293> : thrdpool_schedule
T 21-10-03 23:41:43.921245 [tid : 10039] <thrdpool.c:267> : __thrdpool_schedule
T 21-10-03 23:41:43.921261 [tid : 10039] <thrdpool.c:274> : add entry list to pool task Queue
T 21-10-03 23:41:43.921273 [tid : 10039] <thrdpool.c:293> : thrdpool_schedule
T 21-10-03 23:41:43.921286 [tid : 10039] <thrdpool.c:267> : __thrdpool_schedule
T 21-10-03 23:41:43.921301 [tid : 10039] <thrdpool.c:274> : add entry list to pool task Queue
T 21-10-03 23:41:43.921313 [tid : 10039] <thrdpool.c:293> : thrdpool_schedule
T 21-10-03 23:41:43.921330 [tid : 10039] <thrdpool.c:267> : __thrdpool_schedule
T 21-10-03 23:41:43.921342 [tid : 10039] <thrdpool.c:274> : add entry list to pool task Queue
T 21-10-03 23:41:43.921432 [tid : 10039] <thrdpool.c:293> : thrdpool_schedule
T 21-10-03 23:41:43.921445 [tid : 10039] <thrdpool.c:267> : __thrdpool_schedule
T 21-10-03 23:41:43.921461 [tid : 10039] <thrdpool.c:274> : add entry list to pool task Queue
T 21-10-03 23:41:43.921672 [tid : 10039] <thrdpool.c:293> : thrdpool_schedule
T 21-10-03 23:41:43.921689 [tid : 10039] <thrdpool.c:267> : __thrdpool_schedule
T 21-10-03 23:41:43.921702 [tid : 10039] <thrdpool.c:274> : add entry list to pool task Queue
T 21-10-03 23:41:43.921719 [tid : 10039] <thrdpool.c:293> : thrdpool_schedule
T 21-10-03 23:41:43.921731 [tid : 10039] <thrdpool.c:267> : __thrdpool_schedule
T 21-10-03 23:41:43.921817 [tid : 10039] <thrdpool.c:274> : add entry list to pool task Queue
T 21-10-03 23:41:43.921848 [tid : 10039] <thrdpool.c:293> : thrdpool_schedule
T 21-10-03 23:41:43.921868 [tid : 10039] <thrdpool.c:267> : __thrdpool_schedule
T 21-10-03 23:41:43.921884 [tid : 10039] <thrdpool.c:274> : add entry list to pool task Queue
T 21-10-03 23:41:43.921912 [tid : 10039] <thrdpool.c:293> : thrdpool_schedule
T 21-10-03 23:41:43.921933 [tid : 10039] <thrdpool.c:267> : __thrdpool_schedule
T 21-10-03 23:41:43.922039 [tid : 10039] <thrdpool.c:274> : add entry list to pool task Queue
T 21-10-03 23:41:43.922056 [tid : 10039] <thrdpool.c:293> : thrdpool_schedule
T 21-10-03 23:41:43.922083 [tid : 10039] <thrdpool.c:267> : __thrdpool_schedule
T 21-10-03 23:41:43.922103 [tid : 10039] <thrdpool.c:274> : add entry list to pool task Queue
T 21-10-03 23:41:43.922123 [tid : 10039] <thrdpool.c:293> : thrdpool_schedule
T 21-10-03 23:41:43.922135 [tid : 10039] <thrdpool.c:267> : __thrdpool_schedule
T 21-10-03 23:41:43.922433 [tid : 10039] <thrdpool.c:274> : add entry list to pool task Queue
T 21-10-03 23:41:43.922540 [tid : 10039] <thrdpool.c:293> : thrdpool_schedule
T 21-10-03 23:41:43.922564 [tid : 10039] <thrdpool.c:267> : __thrdpool_schedule
T 21-10-03 23:41:43.922584 [tid : 10039] <thrdpool.c:274> : add entry list to pool task Queue
T 21-10-03 23:41:43.922596 [tid : 10039] <thrdpool.c:293> : thrdpool_schedule
T 21-10-03 23:41:43.922608 [tid : 10039] <thrdpool.c:267> : __thrdpool_schedule
T 21-10-03 23:41:43.922619 [tid : 10039] <thrdpool.c:274> : add entry list to pool task Queue
T 21-10-03 23:41:43.922636 [tid : 10039] <thrdpool.c:293> : thrdpool_schedule
T 21-10-03 23:41:43.922647 [tid : 10039] <thrdpool.c:267> : __thrdpool_schedule
T 21-10-03 23:41:43.922737 [tid : 10039] <thrdpool.c:274> : add entry list to pool task Queue
T 21-10-03 23:41:43.921017 [tid : 10045] <thrdpool.c:54> : __thrdpool_routine
T 21-10-03 23:41:43.922065 [tid : 10060] <thrdpool.c:54> : __thrdpool_routine
T 21-10-03 23:41:43.921708 [tid : 10055] <thrdpool.c:54> : __thrdpool_routine
T 21-10-03 23:41:43.921045 [tid : 10046] <thrdpool.c:54> : __thrdpool_routine
T 21-10-03 23:41:43.922113 [tid : 10061] <thrdpool.c:54> : __thrdpool_routine
T 21-10-03 23:41:43.921420 [tid : 10052] <thrdpool.c:54> : __thrdpool_routine
T 21-10-03 23:41:43.921738 [tid : 10056] <thrdpool.c:54> : __thrdpool_routine
T 21-10-03 23:41:43.921234 [tid : 10048] <thrdpool.c:54> : __thrdpool_routine
T 21-10-03 23:41:43.921830 [tid : 10057] <thrdpool.c:54> : __thrdpool_routine
T 21-10-03 23:41:43.921292 [tid : 10050] <thrdpool.c:54> : __thrdpool_routine
T 21-10-03 23:41:43.922574 [tid : 10062] <thrdpool.c:54> : __thrdpool_routine
T 21-10-03 23:41:43.921452 [tid : 10053] <thrdpool.c:54> : __thrdpool_routine
T 21-10-03 23:41:43.921115 [tid : 10047] <thrdpool.c:54> : __thrdpool_routine
T 21-10-03 23:41:43.921892 [tid : 10058] <thrdpool.c:54> : __thrdpool_routine
T 21-10-03 23:41:43.921660 [tid : 10054] <thrdpool.c:54> : __thrdpool_routine
T 21-10-03 23:41:43.922628 [tid : 10063] <thrdpool.c:54> : __thrdpool_routine
T 21-10-03 23:41:43.921928 [tid : 10059] <thrdpool.c:54> : __thrdpool_routine
T 21-10-03 23:41:43.921257 [tid : 10049] <thrdpool.c:54> : __thrdpool_routine
T 21-10-03 23:41:43.922731 [tid : 10064] <thrdpool.c:54> : __thrdpool_routine
T 21-10-03 23:41:43.921323 [tid : 10051] <thrdpool.c:54> : __thrdpool_routine
T 21-10-03 23:41:43.923690 [tid : 10040] <thrdpool.c:54> : __thrdpool_routine

// todo :
// 1. 为什么会反复thrdpool_schedule， __thrdpool_schedule
// 2. <Communicator.cc:1294> : create handler thread pool 为什么这里需要被创建
// 3. __thrdpool_routine 是哪一个线程池的

T 21-10-03 23:41:43.923840 [tid : 10040] <Executor.cc:90> : Executor::executor_thread_routine
T 21-10-03 23:41:43.923991 [tid : 10040] <WFTaskFactory.inl:581> : __WFThreadTask execute routine

// 最终拿出来的任务session->execute();  就是__WFThreadTask->excute() 就是 routine

T 21-10-03 23:41:43.924009 [tid : 10040] <24_thrd_task_01.cc:28> : add_routine
T 21-10-03 23:41:43.924026 [tid : 10040] <ExecRequest.h:62> : ExecRequest handle
I 21-10-03 23:41:43.924039 [tid : 10040] <WFTask.h:98> : WFThreadTask done
I 21-10-03 23:41:43.924115 [tid : 10040] <24_thrd_task_01.cc:40> : 1 + 2 = 3
```

