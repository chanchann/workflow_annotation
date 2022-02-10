#! https://zhuanlan.zhihu.com/p/465484448
# workflow 源码解析 : Parallel

更加详细的源码注释可看 : https://github.com/chanchann/workflow_annotation

前情回顾 : [series/subtask](./11_subtask.md)

在之前的文章中，我们已经接触过了SeriesWork。

SeriesWork由任务构成，代表一系列任务的串行执行。所有任务结束，则这个series结束。

与SeriesWork对应的ParallelWork类，parallel由series构成，代表若干个series的并行执行。所有series结束，则这个parallel结束。

ParallelWork是一种任务。

根据上述的定义，我们就可以动态或静态的生成任意复杂的工作流了。

## 简单demo

我们先来看一个小demo

```cpp
#include <workflow/Workflow.h>
#include <workflow/WFTaskFactory.h>
#include <workflow/WFFacilities.h>

int main()
{
    ParallelWork *pwork = Workflow::create_parallel_work(nullptr);
    for(int i = 0; i < 3; i++)
    {
        WFTimerTask *task = WFTaskFactory::create_timer_task(3000 * 1000,    
        [](WFTimerTask *timer)
        {
            fprintf(stderr, "timer end\n");
        });
        SeriesWork *series = Workflow::create_series_work(task, nullptr);
        pwork->add_series(series);
    }
    pwork->start();
    getchar();
    return 0;
}
```

在代码中，我们创建一个空的并行任务，并逐个添加series。

我们创建的timer task，并不能直接加入到并行任务里，需要先用它创建一个series。

## 源码分析

## 创建parallel

```cpp
inline ParallelWork *
Workflow::create_parallel_work(parallel_callback_t callback)
{
	return new ParallelWork(std::move(callback));
}

inline ParallelWork *
Workflow::create_parallel_work(SeriesWork *const all_series[], size_t n,
							   parallel_callback_t callback)
{
	return new ParallelWork(all_series, n, std::move(callback));
}
```

第一个接口创建一个空的并行任务，第二个接口用一个series数组创建并行任务。

无论用哪个接口产生的并行任务，在启动之前都可以用ParallelWork的add_series()接口添加series。

## ParallelWork

```cpp

class ParallelWork : public ParallelTask
{
public:
	// 并行是一种任务，启动并行会将其放入串行，并启动串行
	void start()
	{
		assert(!series_of(this));
		Workflow::start_series_work(this, nullptr);
	}

	// 取消并行中的所有任务
	void dismiss();

	void add_series(SeriesWork *series);

	const SeriesWork *series_at(size_t index) const;

	const SeriesWork& operator[] (size_t index) const;

	// 获取并行中串行的个数
	size_t size() const;

	void set_callback(parallel_callback_t callback);
protected:
	virtual SubTask *done();

protected:
	void *context;
	parallel_callback_t callback;

private:
	void expand_buf();
	void dismiss_recursive();

private:
	size_t buf_size;
	SeriesWork **all_series;
	
	// ....
};
```

其中有这几个核心要点:

1. 继承自ParallelTask

parallelwork也是一种task

2. start中可以看到，启动并行会将其放入串行，并启动串行

3. SeriesWork **all_series;

他里面有个series数组

4. add_series

最为常用的就是这个add_series接口

```cpp
void ParallelWork::add_series(SeriesWork *series)
{
	if (this->subtasks_nr == this->buf_size)
		this->expand_buf();

	assert(!series->in_parallel);
	series->in_parallel = true;
	this->all_series[this->subtasks_nr] = series;
	this->subtasks[this->subtasks_nr] = series->first;
	this->subtasks_nr++;
}
```

这里需要注意一下，我们给all_series添加之后，还有父类成员的操作

```cpp
this->subtasks[this->subtasks_nr] = series->first;
this->subtasks_nr++;
```

我们稍后来看

## ParallelTask

先往上看看他的父类

```cpp
class ParallelTask : public SubTask
{
public:
	ParallelTask(SubTask **subtasks, size_t n)
	{
		this->subtasks = subtasks;
		this->subtasks_nr = n;
	}

	SubTask **get_subtasks(size_t *n) const··
	{
		*n = this->subtasks_nr;
		return this->subtasks;
	}

	void set_subtasks(SubTask **subtasks, size_t n)
	{
		this->subtasks = subtasks;
		this->subtasks_nr = n;·
	}

public:
	virtual void dispatch();

protected:
	SubTask **subtasks;
	size_t subtasks_nr;

private:
	size_t nleft;
	friend class SubTask;
};
```

有这几个核心点：

1. 继承自SubTask

确确实实是个task

2. SubTask **subtasks, size_t subtasks_nr;

他这里存着subtasks的数组, 及其个数

注意这里是**subtasks**的数组，是有多少个subtask数组

这里作个图示 

![Image](https://pic4.zhimg.com/80/v2-cf1f123d55c1f2b527ef4c5b41779c85.png)

## 启动

```cpp
class ParallelWork : public ParallelTask
{
public:
	void start()
	{
		// ...
		Workflow::start_series_work(this, nullptr);
	}
	// ...
};
```

此处复习下SeriesWork的操作

```cpp
inline void
Workflow::start_series_work(SubTask *first, series_callback_t callback)
{
	new SeriesWork(first, std::move(callback));
	first->dispatch();
}
```

也就是

```cpp
pwork->dispatch();
```

## parrllel dispatch

dispatch 在 ParallelTask 这层实现

```cpp
void ParallelTask::dispatch()
{
	SubTask **end = this->subtasks + this->subtasks_nr;
	SubTask **p = this->subtasks;

	this->nleft = this->subtasks_nr;
	if (this->nleft != 0)
	{
		do
		{
			(*p)->parent = this;
			(*p)->entry = p;
			(*p)->dispatch();
		} while (++p != end);
	}
	else
		this->subtask_done();
}
```

可以看出，这个nleft的意思就是剩余的**task数组**

当减为0，则这个parallel这个任务完成，走到 subtask_done

否则挨着dispatch这些挂载的子任务

然而，这里并没有nleft的变化，那么必然在每个SubTask的dispath中改变

## dispatch

其实上面这句话并不准确，其实是在每个子任务SubTask完成的subtask_done中对nleft的改变

因为上一篇文章说过，dispath是纯虚函数，是每个task的核心逻辑，需要想要的task自我实现，执行完要调用subtask_done.

比如

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

我们可见他并没有对nleft产生变化

## subtask_done 再探

上一篇我们屏蔽了parallel对细节，这次看个完整版本对subtask_done

首先parallel的dispatch 设置了parent 和 entry

```cpp
do
{
	(*p)->parent = this;
	(*p)->entry = p;
	(*p)->dispatch();
} while (++p != end);
```

```cpp
void SubTask::subtask_done()
{
	SubTask *cur = this;
	ParallelTask *parent;
	SubTask **entry;

	while (1)
	{
		// 保存一下parent和entry
		parent = cur->parent;
		entry = cur->entry;
		cur = cur->done();    // 此处pop下一个task了
		if (cur)   // 如果有下一个task
		{
			// 这里保存的parent和entry就派上了用场
			/*
				void ParallelTask::dispatch()
				{
					SubTask **end = this->subtasks + this->subtasks_nr;
					SubTask **p = this->subtasks;

					this->nleft = this->subtasks_nr;
					if (this->nleft != 0)
					{
						do
						{
							(*p)->parent = this;
							(*p)->entry = p;
							(*p)->dispatch();
						} while (++p != end);
					}
					else
						this->subtask_done();
				}
			*/
			// dispath中外部循环的是subtasks，上面图中的绿色方块
			// 而在subtask_done中，我们不断pop出挂载在绿色方块下面的子task数组中的task
			// 我们在外面只设置了开头的这个task的parent和entry
			// 所以这里每次要挨着把parent和entry都赋值上
			cur->parent = parent;  
			cur->entry = entry;
			if (parent)
				*entry = cur;

			cur->dispatch();  // 不同任务分发至不同的处理请求
		}  // 如果这个series已经没有task了，看看这个task是否为parallel，如果是，则nleft - 1，直到0为止
		// 可以看下面的图
		// 由于Parallel也是一种task，可以很灵活组成各种串并联
		// 我们图例所示，从上到下是包含关系，我们最上面的parallel包含了三个series
		// 第一series中，有两个subtask和一个parallel subtask
		// 第二个有一个subtask，第三个series有一个paralle subtask
		// 第一个series的子parallel subtask又有两个series
		// 具体分析在下面
		else if (parent)   
		{
			if (__sync_sub_and_fetch(&parent->nleft, 1) == 0)
			{
				cur = parent;
				continue;
			}
		}
		break;
	}
}
```

![Image](https://pic4.zhimg.com/80/v2-e3dcb5c0d59a7dae0376dfd4a51b6425.png)

## 看图说话

根据代码，我们重新看图说话理一次，我们整个这个图，也在一个series中（start处可知）

那么第一个执行的就是1号，是个parallel subtask

执行

```cpp
void ParallelTask::dispatch()
{
	SubTask **end = this->subtasks + this->subtasks_nr;
	SubTask **p = this->subtasks;

	this->nleft = this->subtasks_nr;
	if (this->nleft != 0)
	{
		do
		{
			(*p)->parent = this;
			(*p)->entry = p;
			(*p)->dispatch();
		} while (++p != end);
	}
	else
		this->subtask_done();
}
```

其中p是最左边这个series的第一个task，subtask 2

end是最右边series的第一个task，parallel subtask 6

nleft为3，有三个subtask数组

那么外部循环就是相当于循环这三个series的subtask数组

我们先看第一个subtask数组，parent是parallel subtask 1，entry为subtask 2，dispatch

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

假设我们的subtask2为timer task，执行完核心功能sleep，即进入subtask_done

```cpp
void SubTask::subtask_done()
{
	while (1)
	{
		parent = cur->parent;
		entry = cur->entry;
		cur = cur->done();    
		if (cur)   
		{
			cur->parent = parent;
			cur->entry = entry;
			if (parent)
				*entry = cur;

			cur->dispatch();  
		}
		...
		break;
	}
}
```

此处parent是parrallel subtask 1，entry为subtask 2

`cur->done()` 返回了下一个subtask，如果有，那么我们dispatch这个task，然后进入下一个subtask的subtask_done，如果后面都是普通的subtask，那么就一直重复这个过程，直到没有subtask

到了没有subtask的情况，我们之后再分析

那么我们这里返回的不是普通的subtask，而是一个parallel subtask呢，比如我们接下来返回的parallel subtask 4，他又进入了`ParallelTask::dispatch()`, 向下走，去遍历parallel subtask 4下面的两个series的subtask，这个就是个树模型，其实很好理解，就是个递归子树的过程。

```cpp
void SubTask::subtask_done()
{
	// ...
	while (1)
	{
		parent = cur->parent;
		entry = cur->entry;
		cur = cur->done();   
		if (cur)   
		{
			cur->parent = parent;
			cur->entry = entry;
			if (parent)
				*entry = cur;

			cur->dispatch(); 
		}
		else if (parent)  
		{
			if (__sync_sub_and_fetch(&parent->nleft, 1) == 0)
			{
				cur = parent;
				continue;
			}
		}

		break;
	}
}
```

当我们到了没有subtask的情况呢，我们这里会判断是否有parent

比如我们在2，3，4这个series的时候，他的parent是1，那么就nleft - 1，不为0，继续外部循环，（注意我们的外部循环指subtask数组的循环，内部循环指subtask数组中的subtask循环）

到了subtask5的这个series走完了，发现parent是1，有，nleft - 1，不为0，外部循环到第3个subtask数组，发现还是有parent，但是这次减完了，cur变成了parent，往上走了，continue，如果parallel subtask 1所在series下面还有subtask，那么就继续走下一个task

但是这里明显我们只有一个parallel subtask 1，`cur->done()` 返回没有下一个任务了，而且，他又没有parent，没有上一级的parallel，那么就执行结束了

## entry

entry 只有在parallel里才有用到。 

```cpp
(*p)->parent = this;
(*p)->entry = p;
```

```cpp
parent = cur->parent;
entry = cur->entry;
cur = cur->done();   
if (cur)   
{
	cur->parent = parent;
	cur->entry = entry;
	if (parent)
		*entry = cur;

	cur->dispatch(); 
}
```

subtasks数组中的各个子subtask的parent始终指向parent节点的parrallel subtask，entry始终指向子subtasks数组中的第一个p

entry = p -> *entry = *p -> *p = cur

每次要用cur替换掉这个parallel task的子subtask数组的第0个元素，这样保证最后一个元素在parallel callback结束前不被delete掉，目的是parallel的callback中，能拿到所有子subtask的结果, 最后随着Parallel消亡，Series消亡，从而最后一个子subtask消亡。·

## 总结

小小总结一下，这里比较抽象的就是往上走，往下走，但是画个图就很清楚了，当有parallel subtask，那么他就会一直递归下去，执行每一个子树，这相当于是一个多叉树，当执行完这个parallel subtask的所有数组（把nleft减为0的时候），cur变成了parent，然后下一个循环去`cur->done()`，得到上一级的下一个subtask去执行，向下递归，然后向上弹出，就是整个subtask_done的核心。




