#! https://zhuanlan.zhihu.com/p/448434911
# workflow 源码解析 : GoTask

项目源码 : https://github.com/sogou/workflow

更加详细的源码注释可看 : https://github.com/chanchann/workflow_annotation

## GoTask

既然都看了ThreadTask，那就顺便看看他的兄弟 `GoTask`

![Image](https://pic4.zhimg.com/80/v2-ae19782983cd73dd11071fbbedb35d2e.png)

他们都继承自 `ExecRequest`

## Demo

先来看个小demo，看看go task 如何使用

```cpp
#include <workflow/WFHttpServer.h>

void Factorial(int n, protocol::HttpResponse *resp)
{
    unsigned long long factorial = 1;
    for(int i = 1; i <= n; i++)
    {
        factorial *= i;
    }
    resp->append_output_body(std::to_string(factorial));
}

void process(WFHttpTask *server_task)
{
    const char *uri = server_task->get_req()->get_request_uri();
    if (*uri == '/')
        uri++;
    int n = atoi(uri);
    protocol::HttpResponse *resp = server_task->get_resp();
    WFGoTask *go_task = WFTaskFactory::create_go_task("go", Factorial, n, resp);
    **server_task << go_task;
}

int main()
{
    WFHttpServer server(process);

    if (server.start(8888) == 0)
    {
        getchar();
        server.stop();
    }
    return 0;
}
```

## 创建

```cpp
template<class FUNC, class... ARGS>
inline WFGoTask *WFTaskFactory::create_go_task(const std::string& queue_name,
											   FUNC&& func, ARGS&&... args)
{
	auto&& tmp = std::bind(std::forward<FUNC>(func),
						   std::forward<ARGS>(args)...);
	return new __WFGoTask(WFGlobal::get_exec_queue(queue_name),
						  WFGlobal::get_compute_executor(),
						  std::move(tmp));
}
```

其实是new了一个 `__WFGoTask`

```cpp
class __WFGoTask : public WFGoTask
{
protected:
	virtual void execute()
	{
		this->go();
	}

protected:
	std::function<void ()> go;

public:
	__WFGoTask(ExecQueue *queue, Executor *executor,
			   std::function<void ()>&& func) :
		WFGoTask(queue, executor),
		go(std::move(func))
	{
	}
};
```

与 `ThreadTask` 类似，`ThreadTask` 是 `__WFThreadTask` 这一层多了一个 `routine`

`__WFGoTask` 就是多了我们打包bind的callback。

而他的父类也是中规中矩的task，和`ThreadTask`几乎一模一样

```cpp
class WFGoTask : public ExecRequest
{
public:
	void start();
	void dismiss();
	void *user_data;
	int get_state() const { return this->state; }
	int get_error() const { return this->error; }
	void set_callback(std::function<void (WFGoTask *)> cb);
protected:
	virtual SubTask *done();
protected:
	std::function<void (WFGoTask *)> callback;
    virtual ~WFGoTask() { }
public:
	WFGoTask(ExecQueue *queue, Executor *executor);
};
```

只有唯一的区别，`ThreadTask` 是有模板，IN 和 OUT， ThreadTask 依赖 输入输出。

而 `GoTask` 不依赖，而是直接将函数打包成 go 这个callback，等待线程池消费。