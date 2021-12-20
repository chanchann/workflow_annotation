#! https://zhuanlan.zhihu.com/p/415833220
# workflow 源码解析 : ThreadTask

项目源码 : https://github.com/sogou/workflow

更加详细的源码注释可看 : https://github.com/chanchann/workflow_annotation

我们先读源码可以先从thread task入手，相对而言比较容易理解（因为不涉及网络相关的内容）

最主要的是了解workflow中“任务”到底是什么（透露一下，在SubTask.h中定义 

这部分主要涉及kernel中的线程池和队列

## 先写一个简单的加法运算程序 

https://github.com/chanchann/workflow_annotation/blob/main/demos/24_thrd_task/24_thrd_task_01.cc

```cpp
#include <iostream>
#include <workflow/WFTaskFactory.h>
#include <errno.h>

// 直接定义thread_task三要素
// 一个典型的后端程序由三个部分组成，并且完全独立开发。即：程序=协议+算法+任务流。

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

    fprintf(stderr, "%d + %d = %d\n", input->x, input->y, output->res);
}

int main()
{
    using AddFactory = WFThreadTaskFactory<AddInput, AddOutput>;
	AddTask *task = AddFactory::create_thread_task("add_task",
												add_routine,
												callback);
	AddInput *input = task->get_input();

	input->x = 1;
	input->y = 2;

	task->start();

	getchar();
	return 0;
}
```

## thread_task 的使用

首先我们创建一个thread_task 

```cpp
using AddTask = WFThreadTask<AddInput, AddOutput>;
using AddFactory = WFThreadTaskFactory<AddInput, AddOutput>;
AddTask *task = AddFactory::create_thread_task("add_task",
                                            add_routine,
                                            callback);
```

可以看出，其中有几个比较重要的元素

1. WFThreadTask

2. WFThreadTaskFactory

我们先看看工厂是如何产生WFThreadTask的

### WFThreadTaskFactory

```cpp
// WFTaskFactory.h
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

这里继承了ExecRequest

### ExecRequest

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
		...
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

其中最为核心的就是三点

1. 继承自SubTask， ExecSession

Task最终祖先都是SubTask，毋庸置疑，计算任务还具有ExecSession的特性

而网络的task则是继承自SubTask和CommSession, 而CommSession是一次req->resp的交互

2. 在这一层实现了dispatch，核心是完成executor->request

3. 两个重要的成员变量ExecQueue, Executor

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

![Image](https://pic4.zhimg.com/80/v2-ae19782983cd73dd11071fbbedb35d2e.png)
<!-- ![pic](https://github.com/chanchann/workflow_annotation/blob/main/src_analysis/pics/exeReuest.png?raw=true) -->

其中的handle在子类ExecRequest中实现了

而execute在__WFThreadTask实现，到这里才把纯虚函数实现完，才能实例化，所以我们new的是__WFThreadTask.

```cpp
virtual void execute()
{
	this->routine(&this->input, &this->output);
}
```

这里很好理解，在不同的__WFThreadTask中，流程(routine)是不同的，但是他们完成和如何handle的是相同的

```cpp
virtual void handle(int state, int error)
{
	this->state = state;
	this->error = error;
	this->subtask_done();
}
```

任务完成后，走下一个任务。

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

就是在map中，找queue_name对应的queue，如果有就返回，没有就创建并插入。

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

在构造的时候，compute_executor_初始化

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

```cpp
virtual void dispatch()
{
	this->executor->request(this, this->queue);
	...
}
```

ThreadTask 执行的的流程就是`executor->request`

我们用个list就知道，我们的实体需要包裹起来这个list node才能串起来，首先我们看看执行任务实体

```cpp
struct ExecTaskEntry
{
	struct list_head list;
	ExecSession *session;
	thrdpool_t *thrdpool;
};
```

其中的session就是执行execute的实体, __WFThreadTask->execute();

这么一看，ExecRequest dispatch() 就是执行execute，绕了一圈，其实就是为了把执行的控制权交给线程池(executor)

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

主要就是把任务加入队列中等待执行

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

	// 执行此任务
	session->execute();
	session->handle(ES_STATE_FINISHED, 0);
}
```
