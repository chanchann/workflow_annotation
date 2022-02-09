18#! https://zhuanlan.zhihu.com/p/462702955
# workflow 源码解析 : SubTask / Series 

更加详细的源码注释可看 : https://github.com/chanchann/workflow_annotation

### 简单的demo

```cpp
#include <workflow/Workflow.h>
#include <workflow/WFTaskFactory.h>
#include <workflow/WFFacilities.h>

int main()
{
    WFTimerTask *task = WFTaskFactory::create_timer_task(3000 * 1000,    
    [](WFTimerTask *timer)
    {
        fprintf(stderr, "timer end\n");
    });

    task->start();
    getchar();
    return 0;
}
```

我们之前分析了timerTask, 我们先用这个简单的task来分析一番workflow的task是如何组织的。

workflow最为基本的运行逻辑，就是串并联，最为简单的运行模式就是一个Series串联起来依次执行。

### 运行起来

我们先来看看task如何运行起来的，我在创建出task后，start启动

```cpp
class WFTimerTask : public SleepRequest
{
public:
	void start()
	{
		assert(!series_of(this));
		Workflow::start_series_work(this, nullptr);
	}
    ...
};
```

我们所有的task都要依赖在series中进行，所以我们不能让这个裸的task执行，先给他创建series_work

```cpp
inline void
Workflow::start_series_work(SubTask *first, series_callback_t callback)
{
	new SeriesWork(first, std::move(callback));
	first->dispatch();
}
```

## 创建SeriesWork

我们先来观察一下SeriesWork的结构

```cpp
class SeriesWork
{
....
protected:
	void *context;
	series_callback_t callback;
private:
	SubTask *first;  
	SubTask *last;   
	SubTask **queue;  
	int queue_size;
	int front;
	int back;
	bool in_parallel;
	bool canceled;
	std::mutex mutex;
};
```

看的出，是一个queue，也很容易理解，我们把任务装到一个队列里，挨着挨着执行。

我们在new 构造的时候，初始化一下

```cpp
SeriesWork::SeriesWork(SubTask *first, series_callback_t&& cb) :
	callback(std::move(cb))
{
	this->queue = new SubTask *[4];
	this->queue_size = 4;
	this->front = 0;
	this->back = 0;
	this->in_parallel = false;
	this->canceled = false;
	first->set_pointer(this);
	this->first = first;
	this->last = NULL;
	this->context = NULL;
}
```

这里我们注意两点:

1. this->queue = new SubTask *[4]; 

先预留出来4个空间

2. first->set_pointer(this)

把subTask和这个SeriesWork绑定了起来

3. 这里front和back都为0，我们不把first task算在task里，而是用first指针单独存储(首位都较为特殊，都单独存储)

## Subtask

引出本节的重点

```cpp
first->dispatch();
```

我们的SubTask是workflow所有task的爷，他的核心就三个函数

```cpp
class SubTask
{
public:
	virtual void dispatch() = 0;

private:
	virtual SubTask *done() = 0;

protected:
	void subtask_done();
    ...
};
```

其中dispatch和done是纯虚函数，不同的task继承实现不同的逻辑。

## dispatch

dispatch代表任务的发起

在此调用task的核心逻辑，并且要求任务在完成的时候调用subtask_done方法。

我们可以对比两个例子:

就拿我们之前分析过的Timer Task和Go Task

```cpp
// TimerTask
class SleepRequest : public SubTask, public SleepSession
{
	...
	virtual void dispatch()
	{
		if (this->scheduler->sleep(this) < 0)
		{
			this->state = SS_STATE_ERROR;
			this->error = errno;
			this->subtask_done();
		}
	}
	...
}
```

```cpp
// GOTask
class ExecRequest : public SubTask, public ExecSession
{
	...
	virtual void dispatch()
	{
		if (this->executor->request(this, this->queue) < 0)
		{
			this->state = ES_STATE_ERROR;
			this->error = errno;
			this->subtask_done();
		}
	}
	...
}
```

一般在XXRequest这一层继承实现dispatch(), 在上次调用核心逻辑函数，执行完调用subtask_done.

## done

done是任务的完成行为，你可以看到subtask_done里立刻调用了任务的done。(此为执行task逻辑的核心，后面仔细讲解)

我们再拿TimerTask和GoTask对比看看

```cpp
class WFTimerTask : public SleepRequest
{
	...
protected:
	virtual SubTask *done()
	{
		SeriesWork *series = series_of(this);

		if (this->callback)
			this->callback(this);

		delete this;
		return series->pop();
	}
	...
};
```

```cpp
class WFGoTask : public ExecRequest
{
	...
protected:
	virtual SubTask *done()
	{
		SeriesWork *series = series_of(this);

		if (this->callback)
			this->callback(this);

		delete this;
		return series->pop();
	}
	...
};
```

done方法把任务交回实现者，实现者要负责内存的回收，比如可能delete this。

done方法可以返回一个新任务，所以这里我们在series pop出来。(也有return this的情况，这个之后我们遇到再看。)

## subtask_done

接下里是最为核心的task执行逻辑

这里我们先简化一下，不管parallel

```cpp
void SubTask::subtask_done()
{
	SubTask *cur = this;
	...

	while (1)
	{
		...
		cur = cur->done();   
		if (cur)  
		{
			...
			cur->dispatch(); 
		}
		...
		break;
	}
}
```

我们上面的例子，timer task执行，他dispatch后，调用subtask_done,

```cpp
// TimerTask
virtual void dispatch()
{
	if (this->scheduler->sleep(this) < 0)
	{
		this->state = SS_STATE_ERROR;
		this->error = errno;
		this->subtask_done();
	}
}
```

当前任务完成，调用done

```cpp
// TimerTask
virtual SubTask *done()
{
	SeriesWork *series = series_of(this);

	if (this->callback)
		this->callback(this);

	delete this;
	return series->pop();
}
```

返回了series的下一个task继续执行。当然我们这里series只有一个task，pop出去是null，没有可以执行的，就算结束了。

同理，如果是一个series里多个task，这里就下一个task，dipatch，同上执行。

顺便这里说一下，Series最为重要的一个接口就是push_back将SubTask加入到队列中.

```cpp
void SeriesWork::push_back(SubTask *task)
{
	this->mutex.lock();
	task->set_pointer(this);
	this->queue[this->back] = task;
	if (++this->back == this->queue_size)
		this->back = 0;

	if (this->front == this->back)
		this->expand_queue();

	this->mutex.unlock();
}
```