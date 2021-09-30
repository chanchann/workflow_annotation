## thread Task

我们先读源码可以先从thread task入手，相对而言比较容易理解（因为不涉及网络相关的内容）

最主要的是了解workflow中“任务”到底是什么（透露一下，在SubTask.h中定义 

这部分主要涉及kernel中的线程池和队列

## thread_task 的使用

首先我们创建一个thread_task 

```cpp
using MMFactory = WFThreadTaskFactory<MMInput,
                                        MMOutput>;
MMTask *task = MMFactory::create_thread_task("matrix_multiply_task",
                                            matrix_multiply,
												callback);
```

观察下结构，首先看看工厂

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

写一个简单的加法运算程序


