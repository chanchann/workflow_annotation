# workflow_annotation

workflow中文注释

## 总结一下平时水群的问题

1. 自定义协议server/client ssl

https://github.com/sogou/workflow/issues/246

2. task都不阻塞，aio实现

3. srpc 的 compress有压缩算法

4. pread task 支持 文件分块读取

pread / preadv 语义一致

读的块的个数和每个块多大，和操作系统语义一样

5. 裸的tcp连接

src/factory/WFTaskFactory.h

```cpp
template<class REQ, class RESP>
class WFNetworkTaskFactory
{
private:
	using T = WFNetworkTask<REQ, RESP>;

public:
	static T *create_client_task(TransportType type,
								 const std::string& host,
								 unsigned short port,
								 int retry_max,
								 std::function<void (T *)> callback);
    ...
public:
	static T *create_server_task(CommService *service,
								 std::function<void (T *)>& process);
};
```

网络连接的话，工厂是可以由request和response类型作为模版去指定的，然后你的WFNetworkTaskFactory<MyREQ, MyRESP> myfactory;

调用的时候： myfactory->crreate_client_task(TT_TCP_SSL, xxx, xxx, …);

自己写server需要自定义协议的话，可以根据tutorial-10看看

6. What is pipeline server? todo:

7. 为什么用callback

https://github.com/sogou/workflow/issues/170

callback方式比future或用户态协程能给程序带来更高的效率，并且能很好的实现通信与计算的统一

8. callback在什么线程里调用

callback的调用线程必然是处理网络收发和文件IO结果的handler线程（默认数量20）或者计算线程（默认数量等于CPU总核数）

不建议在callback里等待或执行特别复杂的计算。需要等待可以用counter任务进行不占线程的wait，复杂计算则应该包装成计算任务

框架里的一切资源都是使用时分配。如果用户没有用到网络通信，那么所有和通信相关的线程都不会被创建。

9. 为什么我的任务启动之后没有反应

```cpp
int main(void)
{
    ...
    task->start();
    return 0;
}
```

框架中几乎所有调用都是非阻塞的，上面的代码在task启动之后main函数立刻return，并不会等待task的执行结束。正确的做法应该是通过某种方式在唤醒主进程

```cpp
WFFaciliies::WaitGroup wait_group(1);

void callback(WFHttpTask *task)
{
    ....
    wait_group.done();
}

int main(void)
{
    WFHttpTask *task = WFTaskFactory::create_http_task(url, 0, 0, callback);
    task->start();
    wait_group.wait();
    return 0;
}
```

10. 任务对象的生命周期是什么

框架中任何任务（以及SeriesWork），都是以裸指针形式交给用户。所有任务对象的生命周期，是从对象被创建，到对象的callback完成。也就是说callback之后task指针也就失效了，同时被销毁的也包括task里的数据。如果你需要保留数据，可以用std::move()把数据移走，例如我们需要保留http任务中的resp：

```cpp
void http_callback(WFHttpTask *task)
{
    protocol::HttpResponse *resp = task->get_resp();
    protocol::HttpResponse *my_resp = new protocol::HttpResponse(std::move(*resp));
    /* or
    protocol::HttpResponse *my_resp = new protocol::HttpResponse;
    *my_resp = std::move(*resp);
    */
}
```

某些情况下，如果用户创建完任务又不想启动了，那么需要调用task->dismiss()直接销毁任务。

需要特别强调，server的process函数不是callback，server任务的callback发生在回复完成之后，而且默认为nullptr。 ？？？

todo : 此处如何理解不为callback？

11. 为什么SeriesWork（串行）不是一种任务

我们关于串并联的定义是：

- 串行由任务组成

- 并行由串行组成

- 并行是一种任务

显然通过这三句话的定义我们可以递归出任意复杂的串并联结构。

如果把串行也定义为一种任务，串行就可以由多个子串行组成，那么使用起来就很容易陷入混乱。

同样并行只能是若干串行的并，也是为了避免混乱。

其实使用中你会发现，串行本质上就是我们的协程。

12. 更一般的有向无环图

可以使用WFGraphTask，或自己用WFCounterTask来构造。

示例：https://github.com/sogou/workflow/blob/master/tutorial/tutorial-11-graph_task.cc

13. server是在process函数结束后回复请求吗

todo : 写demo 理解

不是。server是在server task所在series没有别的任务之后回复请求。如果你不向这个series里添加任何任务，就相当于process结束之后回复。注意不要在process里等待任务的完成，而应该把这个任务添加到series里。


14. 如何让server在收到请求后等一小段时间再回复

错误的方法是在process里直接sleep。

正确做法，向server所在的series里添加一个timer任务。以http server为例:

```cpp
void process(WFHttpTask *server_task)
{
    WFTimerTask *timer = WFTaskFactory::create_timer_task(100000, nullptr);
    server_task->get_resp()->append_output_body("hello");
    series_of(server_task)->push_back(timer);
}
```

以上代码实现一个100毫秒延迟的http server。一切都是异步执行，等待过程没有线程被占用。

15. 怎么知道回复成功没有

首先回复成功的定义是成功把数据写入tcp缓冲，所以如果回复包很小而且client端没有因为超时等原因关闭了连接，几乎可以认为一定回复成功。

需要查看回复结果，只需给server task设置一个callback，callback里状态码和错误码的定义与client task是一样的，但server task不会出现dns错误

16. 能不能不回复

可以。任何时候调用server task的noreply()方法，那么在原本回复的时机，连接直接关闭。

17. 计算任务的调度规则是什么

我们发现包括WFGoTask在内的所有计算任务，在创建时都需要指定一个计算队列名，这个计算队列名可用于指导我们内部的调度策略。

首先，只要有空闲计算线程可用，任务将实时调起，计算队列名不起作用。

当计算线程无法实时调起每个任务的时候，那么同一队列名下的任务将按FIFO的顺序被调起，而队列与队列之间则是平等对待。

例如，先连续启动n个队列名为A的任务，再连续启动n个队列名为B的任务。那么无论每个任务的cpu耗时分别是多少，也无论计算线程数多少，这两个队列将近倾向于同时执行完毕。这个规律可以扩展到任意队列数量以及任意启动顺序。


18. 为什么使用redis client时无需先建立连接

```cpp
class WFTaskFactory
{
public:
    WFRedisTask *create_redis_task(const std::string& url, int retry_max, redis_callback_t callback);
}
```

其中url的格式为：redis://:password@host:port/dbnum。port默认值为6379，dbnum默认值为0。
workflow的一个重要特点是由框架来管理连接，使用户接口可以极致的精简，并实现最有效的连接复用。框架根据任务的用户名密码以及dbnum，来查找一个可以复用的连接。如果找不到则发起新连接并进行用户登陆，数据库选择等操作。如果是一个新的host，还要进行DNS解析。请求出错还可能retry。这每一个步骤都是异步并且透明的，用户只需要填写自己的request，将任务启动，就可以在callback里得到请求的结果。唯一需要注意的是，每次任务的创建都需要带着password，因为可能随时有登陆的需要。

同样的方法我们可以用来创建mysql任务。但对于有事务需求的mysql，则需要通过我们的WFMySQLConnection来创建任务了，否则无法保证整个事务都在同一个连接上进行。WFMySQLConnection依然能做到连接和认证过程的异步性。

19. 连接的复用规则是什么

大多数情况下，用户使用框架产生的client任务都是无法指定具体连接。框架会有连接的复用策略：

如果同一地址端口有满足条件的空闲连接，从中选择最近一个被释放的那个。即空闲连接的复用是先进后出的。

当前地址端口没有满足条件的空闲连接时：

如果当前并发连接数小于最大值（默认200），立刻发起新连接。

并发连接数已经达到最大值，任务将得到系统错误EAGAIN。

并不是所有相同目标地址和端口上的连接都满足复用条件。例如不同用户名或密码下的数据库连接，就不能复用。

虽然我们的框架无法指定任务要使用的连接，但是我们支持连接上下文的功能。这个功能对于实现有连接状态的server非常重要。相关的内容可以参考关于连接上下文相关文档。

https://github.com/sogou/workflow/blob/master/docs/about-connection-context.md

20. 同一域名下如果有多个IP地址，是否有负载均衡

是的，我们会认为同一域名下的所有目标IP对等，服务能力也相同。因此任何一个请求都会寻找一个从本地看起来负载最轻的目标进行通信，同时也内置了熔断与恢复策略。同一域名下的负载均衡，目标都必须服务在同一端口，而且无法配置不同权重。负载均衡的优先级高于连接复用，也就是说会先选择好通信地址再考虑复用连接问题。

21. 如何实现带权重或不同端口上的负载均衡

可以参考upstream相关文档。upstream还可以实现很多更复杂的服务管理需求。

22. chunked编码的http body如何最高效访问

很多情况下我们使用HttpMessage::get_parsed_body()来获得http消息体。但从效率角度上考虑，我们并不自动为用户解码chunked编码，而是返回原始body。解码chunked编码可以用HttpChunkCursor，例如

```cpp
#include "workflow/HttpUtil.h"

void http_callback(WFHttpTask *task)
{
    protocol::HttpResponse *resp = task->get_resp();
    protocol::HttpChunkCursor cursor(resp);
    const void *chunk;
    size_t size;

    while (cursor.next(&chunk, &size))
    {
        ...
    }
}
```

cursor.next操作每次返回一个chunk的起始位置指针和chunk大小，不进行内存拷贝。使用HttpChunkCursor之前无需判断消息是不是chunk编码，因为非chunk编码也可以认为整体就是一个chunk。

23. 能不能在callback或process里同步等待一个任务完成

不推荐这个做法，因为任何任务都可以串进任务流，无需占用线程等待。如果一定要这样做，可以用我们提供的WFFuture来实现。请不要直接使用std::future，因为我们所有通信的callback和process都在一组线程里完成，使用std::future可能会导致所有线程都陷入等待，引发整体死锁。WFFuture通过动态增加线程的方式来解决这个问题。使用WFFuture还需要注意在任务的callback里把要保留的数据（一般是resp）通过std::move移动到结果里，否则callback之后数据会随着任务一起被销毁。

24. 数据如何在task之间传递

最常见的，同一个series里的任务共享series上下文，通过series的get_context()和set_context()的方法来读取和修改。

而parallel在它的callback里，也可以通过series_at()获到它所包含的各个series（这些series的callback已经被调用，但会在parallel callback之后才被销毁），从而获取它们的上下文。

由于parallel也是一种任务，所以它可以把汇总的结果通过它所在的series context继续传递。

总之，series是协程，series context就是协程的局部变量。parallel是协程的并行，可汇总所有协程的运行结果。

25. Server的stop()操作完成时机

Server的stop()操作是优雅关闭，程序结束之前必须关闭所有server。

stop()由shutdown()和wait_finish()组成，wait_finish会等待所有运行中server task所在series结束。也就是说，你可以在server task回复完成的callback里，继续向series追加任务。stop()操作会等待这些任务的结束。另外，如果你同时开多个server，最好的关闭方法是：

```cpp
int main()
{
    // 一个server对象不能start多次，所以多端口服务需要定义多个server对象
    WFRedisServer server1(process);
    WFRedisServer server2(process);
    server1.start(8080);
    server2.start(8888);
    getchar(); // 输入回车结束
    // 先全部关闭，再等待。
    server1.shutdown();
    server2.shutdown();
    server1.wait_finish();
    server2.wait_finish();
    return 0;
}
```

26. 如何在收到某个特定请求时，结束server

因为server的结束由shutdown()和wait_finish()组成，显然就可以在process里shutdown，在main()里wait_finish，例如：

```cpp
#include <string.h>
#include <atomic>
#include “workflow/WFHttpServer.h”

extern void process(WFHttpTask *task);
WFHttpServer server(process);

void process(WFHttpTask *task) {
    if (strcmp(task->get_req()->get_request_uri(), “/stop”) == 0) {
        static std::atomic<int> flag;
        if (flag++ == 0)
            server.shutdown();
        task->get_resp()->append_output_body(“<html>server stop</html>”);
        return;
    }

    /* Server’s logic */
    //  ....
}

int main() {
    if (server.start(8888) == 0)
        server.wait_finish();

    return 0;
}
```

以上代码实现一个http server，在收到/stop的请求时结束程序。process中的flag是必须的，因为process并发执行，只能有一个线程来调用shutdown操作。

27. Server里需要调用非Workflow框架的异步操作怎么办

还是使用counter。在其它异步框架的回调里，对counter进行count操作。

```cpp
void other_callback(server_task, counter, ...)
{
    server_task->get_resp()->append_output_body(result);
    counter->count();
}

void process(WFHttpTask *server_task)
{
    WFCounterTask *counter = WFTaskFactory::create_counter_task(1, nullptr);
    OtherAsyncTask *other_task = create_other_task(other_callback, server_task, counter);//非workflow框架的任务
    other_task->run();
    series_of(server_task)->push_back(counter);
}
```
注意以上代码里，counter->count()的调用可能先于counter的启动。但无论什么时序，程序都是完全正确的。


28. 个别https站点抓取失败是什么原因

如果浏览器可以访问，但用workflow抓取失败，很大概率是因为站点使用了TLS扩展功能的SNI。可以通过全局配置打开workflow的客户端SNI功能：

```cpp
struct WFGlobalSettings settings = GLOBAL_SETTINGS_DEFAULT;
settings.endpoint_params.use_tls_sni = true;
WORKFLOW_library_init(&settings);
```

开启这个功能是有一定代价的，所有https站点都会启动SNI，相同IP地址但不同域名的访问，将无法复用SSL连接。

因此用户也可以通过upstream功能，只打开对某个确定域名的SNI功能：

```cpp
#Include "workflow/UpstreamManager.h"

int main()
{
    UpstreamManager::upstream_create_weighted_random("www.sogou.com", false);
    struct AddressParams params = ADDRESS_PARAMS_DEFAULT;
    params.endpoint_params.use_tls_sni = true;
    UpstreamManager::upstream_add_server("www.sogou.com", "www.sogou.com", &params);
    ...
}
```

上面的代码把www.sogou.com设置为upstream名，并且加入一个同名的server，同时打开SNI功能。

29. 怎么通过代理服务器访问http资源

方法一（只适用于http任务且无法重定向）：

可以通过代理服务器的地址创建http任务，并重新设置request_uri和Host头。假设我们想通过代理服务器www.proxy.com:8080访问http://www.sogou.com/ ，方法如下：

```cpp
task = WFTaskFactory::create_http_task("http://www.proxy.com:8080", 0, 0, callback);
task->set_request_uri("http://www.sogou.com/");
task->set_header_pair("Host", "www.sogou.com");
```

方法二（通用）：

通过带proxy_url的接口创建http任务：

```cpp
class WFTaskFactory
{
public:
    static WFHttpTask *create_http_task(const std::string& url,
                                        const std::string& proxy_url,
                                        int redirect_max, int retry_max,
                                        http_callback_t callback);
};

```

其中proxy_url的格式为：http://user:passwd@your.proxy.com:port/

proxy只能是"http://"开头，而不能是"https://"。port默认值为80。

这个方法适用于http和https URL的代理，可以重定向，重定向时继续使用该代理服务器。

30. 源码阅读顺序

1. 了解源码中基本调用接口：tutorial是根据概念由浅入深的顺序编排的，先根据主页把tutorial试一下，对应的文档也可以先看完，然后看其他主题的文档，了解基本接口；

2. 了解任务和工厂的关系：找到你平时最常用的一个场景（如果没有的话，可以从最常用的Http协议或其他网络协议入手，看看源码中factory和task的关系；

3. 根据一个任务的生命周期看基本层次：gdb跟着这个场景看看整体调用流程经过那些层次，具体感兴趣的部分可以单独拿出来细读源码；

4. 理解异步资源的并列关系：workflow内部多种异步资源是并列的，包括：网络、CPU、磁盘、计时器和计数器，可以了解下他们在源码中互相是什么关系；

5. 底层具体资源的调度和复用实现：对epoll的封装或者多维队列去实现线程任务的调度，底层都有非常精巧的设计，这些可以在了解workflow整体架构之后深入细看

31. 关于dissmiss

所有的task如果create完，不用的话就dismiss，不然会泄漏(!!不要delete，不是亲手new的，就不要delete)

dismiss 只是在创建完不想启动时调用，正常运行的task在callback之后自动回收

32. 用户自定义协议demo

https://github.com/sogou/workflow/blob/master/docs/tutorial-10-user_defined_protocol.md

自定义时，模仿TutorialRequest, 继承PorotcolMessage

33. HTTP 解析 (todo)

https://github.com/sogou/workflow/issues/267

34. http server机制 (todo)

https://github.com/sogou/workflow/issues/538

35. task定义中的assert

```cpp

template<class REQ, class RESP>
class WFNetworkTask : public CommRequest
{
public:
	/* start(), dismiss() are for client tasks only. */
	void start()
	{
		assert(!series_of(this));
		Workflow::start_series_work(this, nullptr);
	}

	void dismiss()
	{
		assert(!series_of(this));
		delete this;
	}
    ... 
};

这里的assert目的:

task的创建现在都走工厂create的模式，所以create出来的task是series为空的。这个时候，你可以把它串到其他series里，也可以直接start它，会内部检查如果你不在一个串行流上的话，给你创建一个series (todo : ??? 还不是很明白此处)

36. 为何不用shared_ptr

unique_ptr完全拥有所有权，解决的是帮我释放的问题；

shared_ptr拥有共享的所有权，解决的是谁最后负责释放的问题；

weak_ptr完全没有所有权，解决的是在某一时刻能不能获得所有权的问题

在workflow的世界观里，所有我(task)分配的资源都是属于我(task)的，所以理应都由我来管理，并且保证所有资源都会在确定的时刻正确释放，用户只能在我(task)指定的时刻使用这些资源，所以不存在共享所有权的问题，也不存在让用户猜测某个时刻是否有所有权的问题。

37. 内存管理机制

继36再说这个问题

任何任务都会在callback之后被自动内存回收。如果创建的任务不想运行，则需要通过dismiss方法释放。

任务中的数据，例如网络请求的resp，也会随着任务被回收。此时用户可通过std::move()把需要的数据移走。

SeriesWork和ParallelWork是两种框架对象，同样在callback之后被回收。

如果某个series是parallel的一个分支，则将在其所在parallel的callback之后再回收。

38. 什么都不干的task

```cpp
// src/factory/WFTaskFactory.h
static WFEmptyTask *create_empty_task()
{
    return new WFEmptyTask;
}
```

39. WFGoTask 

todo : 用法类似于goroutine

40. WFThreadTask 

todo : 可以带上INPUT/OUTPUT作为模板

41. CounterTask

todo : 


todo : counter 做全局开关

A : 往series里放一个counter task，series就堵住了

等到每一个想往series里加的任务想加的时候，你看看有没有counter，就有先打开这个开关就可以了

这是counter的常见用法哈，延迟很低性能很好.

Q : 如果在series里加入一个不会被执行的counter，那岂不是一直都不往下执行了吗?

在加入下个新任务的时候，主动触发counter

也是用个counter task来挂起series。要继续的时候触发counter就行了

42. restful 接口

框架做restful接口，怎么获取到请求过来的参数:

需要自己在server的process函数里拿到request的path，自己进行解析和分发：task->get_req()->get_request_uri()可以拿到path部分进行不同restful路径的逻辑分发

拿到uri后怎么获得path，query_param ? 另外http怎么取得链接的i地址，端口号 ?

get_request_uri()这个函数可以拿到path和query_param，你需要自己切一下，比如127.0.0.1:1412/a?b=c你可以拿到/a?b=c。

但ip和端口目前没有接口，你可以通过派生实现new_connection做

其他接口看看src/protocol/HttpMessage.h这个文件

todo : write a demo

https://github.com/sogou/workflow/issues/356

43. wait_group

todo : 写个demo

44. 