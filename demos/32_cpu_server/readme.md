## 解析

https://github.com/sogou/workflow/issues/660

所有的网络线程，无论client还是server，都只有一组网络线程。

1. 我们先看client，是怎么产生这一组网络线程的

从01-wget例子看起

```cpp
task = WFTaskFactory::create_http_task(url, REDIRECT_MAX, RETRY_MAX,
                                        wget_callback);
```

```cpp
WFHttpTask *WFTaskFactory::create_http_task(const std::string& url,
											int redirect_max,
											int retry_max,
											http_callback_t callback)
{
	auto *task = new ComplexHttpTask(redirect_max,
									 retry_max,
									 std::move(callback));
    ...
}
```

相当于是new了一个ComplexHttpTask

```cpp
class ComplexHttpTask : public WFComplexClientTask<HttpRequest, HttpResponse>
```

继承自WFComplexClientTask

```cpp
WFComplexClientTask(int retry_max, task_callback_t&& cb):
    WFClientTask<REQ, RESP>(NULL, WFGlobal::get_scheduler(), std::move(cb))
{
    ...
}
```

构造父类的时候，注意`WFGlobal::get_scheduler()`

全局单例的CommScheduler，在创建初始化的时候就创建了相应的`poller_threads`和`handler_threads`

```cpp
int init(size_t poller_threads, size_t handler_threads)
{
    return this->comm.init(poller_threads, handler_threads);
}
```

2. server

-> 表示继承自

```
WFHttpServer(WFServer) -> WFServerBase
```

在启动start的时候

```cpp
int WFServerBase::start(const struct sockaddr *bind_addr, socklen_t addrlen,
						const char *cert_file, const char *key_file)
{
    ...
	if (this->init(bind_addr, addrlen, cert_file, key_file) >= 0)
    ....
}
```

```cpp
int WFServerBase::init(const struct sockaddr *bind_addr, socklen_t addrlen,
					   const char *cert_file, const char *key_file)
{
    ...
	this->scheduler = WFGlobal::get_scheduler();
    ...
}
```

又是这个全局单例，所以用的是同一组网络线程

如果在这一组网络线程中做计算，多个server的话肯定互相影响啊。

用计算任务是一个完美解决方案。


```cpp
class CPUHttpServer : public WFHttpServer
{
public:
    CPUHttpServer(http_process_t proc) :
        WFHttpServer([proc](WFHttpTask *task) {
            WFGoTask *t = WFTaskFactory::create_go_task("server", std::move(proc), task);
            series_of(task)->push_back(t);
        })
    { }
    CPUHttpServer(const struct WFServerParams *params, http_process_t proc) :
        WFHttpServer(params, [proc](WFHttpTask *task) {
            WFGoTask *t = WFTaskFactory::create_go_task("server", std::move(proc), task);
            series_of(task)->push_back(t);
        })
    { }
};
```