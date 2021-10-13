# workflow demo03: 资源池 - WFResourcePool

项目源码 : https://github.com/sogou/workflow

更加详细的源码注释可看 : https://github.com/chanchann/workflow_annotation

相关文档 : https://github.com/sogou/workflow/blob/master/docs/about-conditional.md

## 需要的场景

任务运行时需要先从某个池子里获得一个资源。任务运行结束，则会把资源放回池子，让下一个需要资源的任务运行。

网络通信时需要对某一个或一些通信目标做总的并发度限制，但又不希望占用线程等待。

我们有许多随机到达的任务，处在不同的series里。但这些任务必须串行的运行。

## 接口

```cpp

class WFResourcePool
{
public:
	WFConditional *get(SubTask *task, void **resbuf);
	WFConditional *get(SubTask *task);
	void post(void *res);

public:
	struct Data
	{
		void *pop() { return this->pool->pop(); }
		void push(void *res) { this->pool->push(res); }

		void **res;
		long value;
		size_t index;
		struct list_head wait_list;
		std::mutex mutex;
		WFResourcePool *pool;
	};

protected:
	virtual void *pop()
	{
		return this->data.res[this->data.index++];
	}

	virtual void push(void *res)
	{
		this->data.res[--this->data.index] = res;
	}

protected:
	struct Data data;

private:
	void create(size_t n);

public:
	WFResourcePool(void *const *res, size_t n);
	WFResourcePool(size_t n);
	virtual ~WFResourcePool() { delete []this->data.res; }
};
```

## get/post 用法

最为重要常用的接口就是get / post

我们先看一个demo，了解如何用get和post

### demo00 

代码地址 : https://github.com/chanchann/workflow_annotation/blob/main/demos/26_resource_pool/26_resource_pool_03.cc

```cpp
#include <workflow/Workflow.h>
#include <workflow/WFTaskFactory.h>
#include <workflow/WFFacilities.h>
#include <vector>
#include <signal.h>

using namespace protocol;

static WFFacilities::WaitGroup wait_group(1);

void sig_handler(int signo)
{
    wait_group.done();
}

int main()
{
    signal(SIGINT, sig_handler);
    WFResourcePool pool(1);
    WFHttpTask *task1 =
        WFTaskFactory::create_http_task("http://www.baidu.com", 4, 2,
        [&pool](WFHttpTask *task)
        {
            // 当用户对资源使用完毕（一般在任务callback里），需要通过post()接口把资源放回池子。
            fprintf(stderr, "task 1 resp status : %s\n",
                    task->get_resp()->get_status_code());
            pool.post(nullptr);
        });
    WFHttpTask *task2 =
        WFTaskFactory::create_http_task("http://www.bing.com", 4, 2,
        [&pool](WFHttpTask *task)
        {
            fprintf(stderr, "task 2 resp status : %s\n",
                    task->get_resp()->get_status_code());
            pool.post(nullptr);
        });

    // get()接口，把任务打包成一个conditional。conditional是一个条件任务，条件满足时运行其包装的任务。
    // get()接口可包含第二个参数是一个void **resbuf，用于保存所获得的资源。
    // 新版代码接口中这里无需第二个参数
    // WFConditional *cond_task_1 = pool.get(task1); 
    // WFConditional *cond_task_2 = pool.get(task2);

    // 注意conditional是在它被执行时去尝试获得资源的，而不是在它被创建的时候
    WFConditional *cond_task_1 = pool.get(task1, &task1->user_data); 
    WFConditional *cond_task_2 = pool.get(task2, &task2->user_data);

    // cond_task_1先创建，等待task2结束后才运行
    // 这里并不会出现cond_task_2卡死，因为conditional是在执行时才获得资源的。
    cond_task_2->start();
    // wait for t2 finish here.
    fprintf(stderr, "pile here\n");  // 因为是异步的，所以先出现，不要被wait for t2 finish here.迷惑了
    cond_task_1->start();
    wait_group.wait();

    return 0;
}
```

我们这个例子中，就是限制住资源池为1，所以两个任务只能串行，然后cond_task_2先运行，在执行时才是真的get资源，运行完，在callback中post回去，我们这`post(nullptr)`相当于是一个计数开关一样。然后第二个任务在资源池有资源再运行。

我们就这个代码来分析一下如何实现这个效果。

首先，看看构造函数，这是个什么东东。

## 构造函数

```cpp
WFResourcePool::WFResourcePool(size_t n)
{
	this->create(n);
	memset(this->data.res, 0, n * sizeof (void *));
}

void WFResourcePool::create(size_t n)
{
	this->data.res = new void *[n];
	this->data.value = n;
	this->data.index = 0;
	INIT_LIST_HEAD(&this->data.wait_list);
	this->data.pool = this;
}
```

可以看出，这个pool就是维护了一个data, 这个data就是一个wait_list

## get

用户使用get()接口，把任务打包成一个conditional。conditional是一个条件任务，条件满足时运行其包装的任务。

```cpp
WFConditional *WFResourcePool::get(SubTask *task, void **resbuf)
{
	return new __WFConditional(task, resbuf, &this->data);
}
```

可见我们的在get的时候，就是先new了一个`__WFConditional`

## __WFConditional

```cpp
class __WFConditional : public WFConditional
{
public:
	struct list_head list;
	struct WFResourcePool::Data *data;

public:
	virtual void dispatch();
	virtual void signal(void *res) { }

public:
	__WFConditional(SubTask *task, void **resbuf,
					struct WFResourcePool::Data *data) :
		WFConditional(task, resbuf)
	{
		this->data = data;
	}
};
```

这里有几个核心点

1. 继承自WFConditional(详见WFConditional解析模块的文章)

2. 是用来被串起来的class

3. struct WFResourcePool::Data *data;

4. 实现重写了dispatch和signal(且这个为空)

5. 这个__WFConditional的data(wait_list)就是我们资源池的data(wait_list)

细节我们先忽略，我们知道了这里我们`WFConditional *cond_task_1`就是这里new的`__WFConditional`

当我们`cond_task_2->start();`，这个`__WFConditional dispatch`的时候

## __WFConditional dispatch

```cpp
void __WFConditional::dispatch()
{
	struct WFResourcePool::Data *data = this->data;

	data->mutex.lock();
	if (--data->value >= 0)
		this->WFConditional::signal(data->pop());
	else
		list_add_tail(&this->list, &data->wait_list);

	data->mutex.unlock();
	this->WFConditional::dispatch();
}
```

可以看出，他在这里判断还是否有资源，如果有，就signal pop出来的这个任务，否则就加入到wait_list中

- 我们这里`cond_task_2` 先执行，到这里发现我们有资源(`--data->value >= 0`)

我们`this->WFConditional::signal(data->pop());`

这里有两步

1. data->pop()

2. WFConditional::signal

- 我们这里`cond_task_1` 后执行，到这里发现我们没资源(`--data->value >= 0 is false`)

我们就把他串到资源池的wait_list中去`list_add_tail(&this->list, &data->wait_list);`

最后我们都要执行`this->WFConditional::dispatch();`

## data->pop()

```cpp
struct Data
{
    void *pop() { return this->pool->pop(); }
    ...
};
```

```cpp
virtual void *pop()
{
    return this->data.res[this->data.index++];
}
```

我们这里的demo其实就是把资源池当个cnt pool来用，里面的元素就是为`void *`, 我们就是利用value判断是否有资源

这里就是一个FIFO队列，我return当前，然后移动index

那么这里问题来了 : 这里index总不能无限增大呗，等后面配合着push看看怎么控制的。

## WFConditional::signal

```cpp
virtual void signal(void *msg)
{
    *this->msgbuf = msg;
    if (this->flag.exchange(true))
        this->subtask_done();
}
```

这里请注意

https://en.cppreference.com/w/cpp/atomic/atomic/exchange

这里`flag.exchange(true)` 的`return value` 是 `The value of the atomic variable before the call.`

这里是没有走到subtask_done的(todo : 这里为何这样设计)

## WFConditional::dispatch()

```cpp
virtual void dispatch()
{
    series_of(this)->push_front(this->task);
    this->task = NULL;
    if (this->flag.exchange(true))
        this->subtask_done();
}
```

如果是先执行的`cond_task_2` 走到这里，在这之前先插入我们真正需要执行的task(http_task task2)

然后此时`exchange`才算结束。

如果是后执行的`cond_task_1`，已经加入了wait_list队列中。