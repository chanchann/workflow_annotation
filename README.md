## workflow中文注释 / demo / 问题解答

## workflow项目地址

https://github.com/sogou/workflow

## 更多的 demos/ examples

[more examples](./demos/)

## 源码解析系列

### Http server 

- [Http Server 解析1](./src_analysis/22_http_server_00.md)

- [Http Server 解析2](./src_analysis/22_http_server_01.md)

- [Http Server 解析3](./src_analysis/22_http_server_02.md)

### Http client

- [http task 基础结构](./src_analysis/18_http_01.md)

### 非网络Task

网络task相对较为复杂，可以先从这几个task入手，了解task的继承结构。

- [Thread Task](./src_analysis/12_thread_task.md)

- [Go Task](./src_analysis/12_go_task.md)

- [Timer Task](./src_analysis/15_timer_task.md)

## wf基础数据结构

- [基础数据结构 list](./src_analysis/24_list.md)

- [基础数据结构 rbtree](./src_analysis/25_rb_tree.md)

### wf核心

- [SubTask/Series机制](./src_analysis/11_subtask.md)

- [Parallel机制](./src_analysis/11_parallel.md)

## json parser

- [Json parser : part1 parse](./src_analysis/23_json_01_parse.md)

- [Json parser : part2 access](./src_analysis/23_json_01_parse.md)

### FAQ 中的解析解析

- [faq 62 : 原始的task在series没有结束的时候是不会被销毁吗?](./src_analysis/faq_62.md)

### 解析杂记

- [workflow杂记00 : 分析一次http性能改动](./src_analysis/other_00_http_improve.md)

- [workflow杂记01 : 分析一次cache中lock的改动](./src_analysis/other_01_cache_lock.md)

- [workflow杂记02 : dns的优化](./src_analysis/other_02_dns_opt.md)

- [workflow杂记03 : cache size 分析](./src_analysis/other_03_cache_size.md)

- [workflow杂记04 : 分析Defer deleting server task to increase performance](./src_analysis/other_04_task_defer_delete.md)

### 其他未整理的文章

- [文章集合](./src_analysis/)

## 预先声明

demos 中

```cpp
#include <workflow/logger.h>
```

并非原生workflow所有，本版本添加上日志，方便学习观察流程

用户用原生workflow跑demos，可以把log改fprintf

## 总结一下平时水群的问题

1. 自定义协议server/client ssl

https://github.com/sogou/workflow/issues/246

2. task都不阻塞，磁盘IO任务利用了Linux底层的aio接口，文件读取完全异步。

todo : 此处需要源码细节分析

3. workflow 是否有压缩算法

无，but

srpc 的 compress有压缩算法

https://github.com/sogou/srpc/tree/master/src/compress

wfrest也支持gzip 和 br 压缩

可见 https://github.com/wfrest/wfrest

4. pread task 支持 文件分块读取

pread / preadv 语义一致

读的块的个数和每个块多大，和操作系统语义一样

5. tcp server

见tutorial 10

https://github.com/sogou/workflow/issues/40

6. What is pipeline server?

todo:

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

需要特别强调，server的process函数不是callback，server任务的callback发生在回复完成之后，而且默认为nullptr

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

[code](./demos/16_graph)

13. server是在process函数结束后回复请求吗

不是。server是在server task所在series没有别的任务之后回复请求。

如果你不向这个series里添加任何任务，就相当于process结束之后回复。

注意不要在process里等待任务的完成，而应该把这个任务添加到series里。

[code](./demos/11_life_cycle)

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

[code](./demos/07_http/http_echo_defer.cc)

15. 怎么知道回复成功没有

首先回复成功的定义是成功把数据写入tcp缓冲，所以如果回复包很小而且client端没有因为超时等原因关闭了连接，几乎可以认为一定回复成功。

需要查看回复结果，只需给server task设置一个callback，callback里状态码和错误码的定义与client task是一样的，但server task不会出现dns错误

16. 能不能不回复

可以。任何时候调用server task的noreply()方法，那么在原本回复的时机，连接直接关闭。

[code](./demos/07_http/http_no_reply.cc)

17. 计算任务的调度规则是什么

包括WFGoTask在内的所有计算任务，在创建时都需要指定一个计算队列名，这个计算队列名可用于指导我们内部的调度策略。

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

很多情况下我们使用HttpMessage::get_parsed_body()来获得http消息体。

但从效率角度上考虑，我们并不自动为用户解码chunked编码，而是返回原始body。

解码chunked编码可以用HttpChunkCursor，例如

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

cursor.next操作每次返回一个chunk的起始位置指针和chunk大小，不进行内存拷贝。

使用HttpChunkCursor之前无需判断消息是不是chunk编码，因为非chunk编码也可以认为整体就是一个chunk。

23. 能不能在callback或process里同步等待一个任务完成

不推荐这个做法，因为任何任务都可以串进任务流，无需占用线程等待。

如果一定要这样做，可以用我们提供的WFFuture来实现。请不要直接使用std::future，因为我们所有通信的callback和process都在一组线程里完成，使用std::future可能会导致所有线程都陷入等待，引发整体死锁。

WFFuture通过动态增加线程的方式来解决这个问题。

使用WFFuture还需要注意在任务的callback里把要保留的数据（一般是resp）通过std::move移动到结果里，否则callback之后数据会随着任务一起被销毁。

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

1） 了解源码中基本调用接口：tutorial是根据概念由浅入深的顺序编排的，先根据主页把tutorial试一下，对应的文档也可以先看完，然后看其他主题的文档，了解基本接口；

2）了解任务和工厂的关系：找到你平时最常用的一个场景（如果没有的话，可以从最常用的Http协议或其他网络协议入手，看看源码中factory和task的关系；

3) 根据一个任务的生命周期看基本层次：gdb跟着这个场景看看整体调用流程经过那些层次，具体感兴趣的部分可以单独拿出来细读源码；

4) 理解异步资源的并列关系：workflow内部多种异步资源是并列的，包括：网络、CPU、磁盘、计时器和计数器，可以了解下他们在源码中互相是什么关系；

5) 底层具体资源的调度和复用实现：对epoll的封装或者多维队列去实现线程任务的调度，底层都有非常精巧的设计，这些可以在了解workflow整体架构之后深入细看

also :

1) 先跑一下官方例子

2) 看WFTaskFactory.h中都有什么任务，了解每个任务的作用

3) 从thread task入手，相对而言比较容易理解（因为不涉及网络相关的内容），最主要的是了解workflow中“任务”到底是什么（透露一下，在SubTask.h中定义）；这部分主要涉及kernel中的线程池和队列

4) 然后开始看Timer task，了解下怎么实现一个异步的定时器，这个时候就开始接触Communicator和Session了（这个是个比较核心的内容）

5) 再看网络相关的task，建议直接入手WFComplexClientTask，http task或redis task只是协议不同，本质都是一个WFComplexClientTask，了解这个，就基本了解网络相关的任务了

https://zhuanlan.zhihu.com/p/359104170

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

在加入下个新任务的时候，主动触发counter

也是用个counter task来挂起series。要继续的时候触发counter就行了

42. restful 接口

框架做restful接口，怎么获取到请求过来的参数:

需要自己在server的process函数里拿到request的path，自己进行解析和分发：task->get_req()->get_request_uri()可以拿到path部分进行不同restful路径的逻辑分发

那么

拿到uri后怎么获得path，query_param ? 另外http怎么取得链接的ip地址，端口号 ?

get_request_uri()这个函数可以拿到path和query_param，你需要自己切一下，比如127.0.0.1:1412/a?b=c你可以拿到/a?b=c。

而URIParser 提供的接口中，都是需要query而不是get_request_uri获得的path+query_param

```cpp
static std::map<std::string, std::vector<std::string>>
split_query_strict(const std::string &query);

static std::map<std::string, std::string>
split_query(const std::string &query);
```

所以我们需要自己动手切，简单做法是:

```cpp
const char *uri = req->get_request_uri();
const char *p = uri;

printf("Request-URI: %s\n", uri);
while (*p && *p != '?')
    p++;

std::string abs_path(uri, p - uri);
abs_path = root + abs_path;

std::string query(p+1);  
```

demo : 

https://github.com/chanchann/workflow_annotation/blob/main/demos/27_parse_uri/27_request_uri_split.cc

但ip和端口目前没有接口，你可以通过派生实现new_connection做

其他接口看看src/protocol/HttpMessage.h这个文件

todo : write a demo

https://github.com/sogou/workflow/issues/356

43. wait_group

todo : 写个demo

44. 只用wf做线程池，任务调度

只关注WFGoTask就够了，用队列名来管理你想要的调度就行

45. 基于counter实现多入边节点(node为什么基于WFCounterTask实现)

https://github.com/sogou/workflow/issues/196

46. 动态创建多个task，又希望这些task能被顺序的执行来避免多线程竞争

https://github.com/sogou/workflow/issues/301

其实需要的就是一个不会自动结束的series，你可以向这个series里不断的增加任务，这样子这些任务就可以顺序的被执行。

方法是依次push_back任务时，除了push_back当前任务，还需要再push_back一个目标值为1的counter任务。

接下来，上一个counter打开，让当前任务可以被拉起。counter相当于一个塞子，用于堵住series，让series不会自动结束。

其实我们series的push和pop操作都是加锁的，也就是为了用户可以实现这个功能。

注意 : 在callback里面push计数器在并发访问的时候就有问题


```cpp
void mytask_callback(MyTask *task)
{
    if (/* series里没有其他任务了*/ )
    {
        // 如果这个地方拿到series的人直接push一个task，那就堵住了
        WFCounterTask *counter = WFTaskFactory::create_counter_task("COUNTER_A", 1, nullptr);
        series->push_back(counter);
    }
    ...
}
```

47. BlockSeries

Q : BlockSeries的实现 只管往series添加task和counter就行吧，不需要担心series的内存占用，也就是说task执行完了会自动从series里移除对吧？

A : task执行完都会被销毁，和在哪个执行体里没关系的（比如自动给你分配的series或者你去指定的series）

48. 内存分配

内存分配交给jemalloc就够了，wf本身解决调度和异步资源

49. post

Q : 使用http task发送post的话，是不是获取到task的req对象，然后用req的append方法追加数据就行了？同时设置头部方法为post？

A : 是的，content length如果你不填，会自动帮你填

50. 内部有用 workflow作为算法模型的service 吗

有的。目前用WThreadTask去做算法封装。然后server接到的任务是WFNetworkTask，做计算就起个WFThreadTask，那workflow就可以帮你做调度，不用担心卡住server接不到新请求。

这是workflow计算通信融为一体的非常典型的用法

51. wf的一些考虑

workflow做了很多很多事情，核心就是在解决c++内存管理问题，workflow的世界里东西生命周期是很明确的，有点像“用户态<->框架态<->内核态”这样划分，所有权交回给用户的时候，生命周期是完全你管的，并且回调函数之后内存会被释放干净；而异步任务执行期间你是不能干预的，如果你有想干预的节点，自行拆开两个task。

而不像nginx模块开发，写个模块，处理阶段的函数还要关心request的body里的指针你要维护好，读没读别搞错要不等下主框架就容易搞错这种情况。

workflow也有个约定是谁申请谁释放，并且内部对所有异步资源的创建和管理都封装了起来，免去了开发者的操作的麻烦

52. server task中的process也是并发的

yes

process并发的意思是多个请求来了会有多个线程在执行你的process，但每个process里只有一个线程在执行

context是连接上下文，每个连接有一个context

53. redis-cli 建立连接的过程是怎样的

https://github.com/sogou/workflow/issues/330

Q : mysql-cli我单步调试到Workflow::start_series_work 的时候会创建一个WFRouterTask,这个route是在哪个环节添加的任务？

A : 解析url

54. 关于错误码体系

你从task->error拿到的错误码是workflow定义的；

errno是系统用的，或者workflow框架用来标记系统的错误；

55. httpServerTask，start之后，具体在哪触发process callback呢？

linux下为例子，是使用epoll的，收到数据并切下消息就会触发process。

56. msgqueue

https://github.com/sogou/workflow/issues/162

https://github.com/sogou/workflow/issues/349

https://github.com/sogou/workflow/issues/353

相关解析看src_analysis dir

57. blank

58. 如何看poller相关代码

https://github.com/sogou/workflow/issues/351

https://github.com/sogou/workflow/issues/297

https://github.com/sogou/workflow/issues/166

相关解析看src_analysis dir

59. 离散内存和zero_copy一些思考

https://zhuanlan.zhihu.com/p/141485764

A : 离散内存其实作用范围很特殊，目前在序列化和io结合的地方用比较有效果，为什么呢，因为序列化的时候，往往不知道目标内存块的大小，一边序列化，一边内存块在增长，以往我们都是扩展内存块，加上内存拷贝来解决这个问题的，但是内，由于io某些os实现了gantherio，这东西能一次性发送多块内存到tcp栈，有人就动起来小脑筋，嗯哼，那我扩展内存的时候，就表加大内存块的大小，而是增加内存块的数量，这样就减少了多次合并内存的内存拷贝了吗，真是太机智了，想想都小激动。这块目前我看做的比较好的开源的就是https://www.boost.org/doc/libs/1_75_0/libs/beast/doc/html/beast/ref/boost__beast__multi_buffer.html

B: srpc当时的改动也是类似这样的考虑，内部是个多块的固定大小的内存链表～然后序列化和解压就一键打通了

其实主要压缩的话，有时候我们没办法知道原始内存应该多大。那些压缩算法算出来的理论最大值太大了，不可能跟着他们的去malloc

60. 目前每次发起一个请求都需要create_http_task下，有没有什么方式来重用这个http_task？

A : 不需要重用

长连接不需要每次connect，task和连接是两码事

Q : 如果发起第二次请求，重新create_http_task么？

对， 内部如果有空闲连接会帮你复用

再谈这个task重用问题:

```cpp

class WFGoTask : public ExecRequest
{
public:
	void start()
	{
		assert(!series_of(this));
		Workflow::start_series_work(this, nullptr);
	}
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

此处done了就delete this了

设计上是不需要重用因为task只是一个很轻量级的单位（比用户态栈要轻多了）

如果非要重用，需要改许多许多东西（因为不符合现在的设计理念。

61. create了5个go_task,放到series_work中执行,然后退出. gdb调试发现有20+个线程created. 这个线程为啥created这么多?

这个是默认线程池的大小

1) go task是计算任务，一旦有计算任务，就会创建计算线程池；

2) 默认计算线程池会开本机核数相等的线程数；大小可以改，这个你可以看看文档怎么改配置

3) 没有用到的资源不会创建（比如网络线程池。或者如果只用网络，不会创建计算线程资源；

62. proxy

Q : 看了下proxy的教程，原始的task在serie没有结束的时候是不会被销毁的，对吗?

见[analysis](./src_analysis/faq_62.md)

63. https 代理

https://github.com/sogou/workflow/issues/379

64. task之间传递结果,有什么标准做法吗?

task有一个user_data，以及series上的context，都是用来传递“非框架管理生命周期”的所有数据的。

user_data属于task，所以它使用的时期是task开始前和它的callback里；

context属于seires，所以它的使用时期是整个任务流所有task可以共享的；

65. tutorial03里面，series和http_task和redis_task有可能会被不同的线程执行吗？

一个series只能保证串行执行，不能保证同一个线程

66. 作为http client ,create_http_task.之后在哪里 设置post data呢?

req->append_output_body()

67. 服务端收到两个先后请求，在WF中这两个请求代表两个series，他们之间的上下文怎么使用？

给server的那个function本身就是个自带上下文的结构哈，举个例子你可以bind某个类的成员函数进去，这个类就是你多个请求可以共享的上下文

Q : bind类成员确实可以解决多个请求的上下文需求，但是并发情况下，多组的多个上下文请求似乎还是无法满足

A : 什么是多组的多个上下文呢？是不是比如消息多条为一组？感觉这种需要协议层面去解决吧，server怎么知道谁属于一组呢？一般不是协议里拆开写着我这个是frame1-frame2-frameOK之类的？

Q : 多组的意思是clientA登录会发消息A->B->C(三个)；clientB登录也会发A->B->C(三个)，这就是两组

暂时是以client的地址+端口组作为key，context存redis解决

A : 需要全局维护一个数据结构（存redis道理上也是一样的），用key做区分，就是正确的做法了。怎么区分（client地址）、怎么聚合（A->B->C

也许传统的方式是一个fd上有一个上下文，但这种模式连接就不能复用了哈，对client来说并发度就不如复用的好，考虑点不一样

Q : client端的context复用使用series.context 就挺好；但是server端 series在消息间是割裂的

A : 其实client端的context是任务流的上下文，只是因为你把A->B->C串一起了，所以才会看到连续的；server的series是被动创建，是一直还在的，你可以往当前的series后面放东西，比如做proxy，这些也是共享series.context，也是连续的

只是本质上你可以主动组织client task，却不能组织server task。因为server总是个被动的角色

Q : 只是本质上你可以主动组织client task，却不能组织server task。因为server总是个被动的角色

A : 可是如果一个fd上收到的消息切下来产生一个server task，server task作为proxy要往后传、proxy回来我再回给用户。那不就冲突了么

我这个当前的series，是串下一个server task呢，还是proxy task呢

毕竟series只能串行执行，但这些都是可以并行的

我感觉这个现在就可以解决了呀？自行按client的信息拆分。如果通用点来说，你要收的消息是无论哪个client过来，只要一个A、一个B、一个C来了你就可以做接下来的事情，那么这个区分的逻辑就又不一样了，这都是开发者自己的逻辑哈


Q : 我说这个问题，就是说WF是异步框架，为了解决server端的这个问题，我又引入了新的依赖redis解决多线程问题，有点冗余

A : 你可以全局一个map<string, ctx>

Q : map不会有并发问题吗

A : 除了插入新key需要自己加锁，好像你本来cleint发过来的东西就是串行的吧？拿出来用没事

用redis肯定读都得加锁、还得跨进程（maybe跨机器），更费劲哈。本地存可以的

Q : 想着如果wf 能够提供socket的context这样就方便许多

A : 如果你只需要socket的context，是有的，有个get_connection()接口，主要是二次开发的人用的哈。client都用（因为client发消息往往需要握手、认证、再发），server你可以做二次开发，自己往connection上放应该也可以。你可以看看

Q : get_connection()和socket似乎不对应，之前我试过connection的context，connection很容易就释放掉了

A : connetciont就是socket，如果你是短连接那当然释放了

issue : server端如何复用一组消息的context？

https://github.com/sogou/workflow/issues/410

68. 当收到Transfer-Encoding: chunked包的时候，http_parser_get_body只返回了原始数据，没有重新组包，导致夹杂chunked的包不能使用。

见22 : https://github.com/sogou/workflow/issues/170

为了考虑效率不会自动解chunk，你有接口可以调

69. 创建好任务流之后.在任务流执行完之前,如何优雅的提前退出这个任务流?

https://github.com/sogou/workflow/issues/422

70. SeriesWork 只要没有结束,可以一致push_back 新的 task 运行, ParalleWork 一旦Start之后,后面addSeries 的任务都不执行.

有没有办法让ParalleWork也能动态执行后面添加的taskflow呢

A : parallel start后，下次再拿到parallel已经是parallel callback了（这里估计是个const parallel），应该没有时机add series吧

Q : 我现在的场景,有一个线程常驻 搜索局域网设备. 每搜到一个设备 就需要开启一个SeriesWork来做升级. 因为需要支持批量并行升级.就想在ParalleWork里面动态执行seriesWork

A : series本身就是并行跑的

parallel本身也是多个series组成，只是会等所有series结束之后你拿到那个callback的时机。除非你需要这个时机否则不需要套一个parallel

直接用SeriesWork也可

71. series内串行、多个series可以并行

72. WFHttpServer调了stop，会不会等待所有的http task执行完成？还是说会直接终止掉所有的？

todo :

A : 会终止等待，任务会拿到aborted的状态

Q : 有没有方法等所有的task结束再退出？

这样的话，你可以：

1) 全局记录两个值：count=0；stopped=false；

2) server每收到一个task，如果stopped不为true：计数count++；否则不处理结果直接拒绝；

3) server task设置callback，这个callback里count—；

4) server继续做事情，包括异步请求别的地方等等；

5) 处理完之后，server会自动回复，回复完对方才会调用到刚才设置的callback；

6) 你想stop的时候，stopped=true，然后等这个count变回0就行。

73. workflow的dns解析异步

https://github.com/sogou/workflow/issues/462

74. server task 生命周期相关

**任务(以及series)的生命周期在callback之后结束**

任务里的数据，例如一个网络的resp，在callback之后被内存回收，如果需要保留，可以通过std::move把数据移走

https://zhuanlan.zhihu.com/p/391013518

75. server和client端有没有连接建立 和 连接断开的回调函数 开放给用户？ 另外，如果想做client与server端订阅推送，能支持吗

todo

workflow的概念是这样，你是不能直接管理一个fd的概念的，如果你对连接有保持或者断开的需求，你可以keepalive去决定

连接都是默认建立的，出错了会自动断开，跑一个mysql任务，建立连接这些都是透明的

然后订阅：可以的，有一个first_timeout接口你可以看看文档。

大概意思是client发请求给server的话，server第一次给我回复的超时是多久。如果我们允许订阅最多1小时的信息，这个值为1小时，连接会一直保持直到server有数据给我

Q : 我的场景是 client通过tcp连接上server后，server可以保存这个session，然后后续会持续往这个session推数据流

A : 用websocket，下次收到数据继续订阅就可了

Q : websocket应该是可以的, 不过感觉tcp会快一些，偏底层一点

A : websocket本身除了http握手，其他都是tcp，包头非常小

这个与网络传输和处理速度相比，基本不值一提。

76. task和callback都是一次性的

77. 在workflow里面怎么统计某个消息，从请求进来，到应答出去  在系统内部的穿透延时？

todo 

这个workflow没有，你有需要可以自己做。workflow的定位是这些都可以外部开发的

srpc有个span可以统计延时，你可以看看

https://github.com/sogou/srpc/issues/86

可以做抽样打log检查请求耗时这样

78. 关于项目内c风格代码(kernel)

kernel里的代码是c风格的，一方面是性能快，另一方面是某些模块比如communicator，是有出处的（从内部存储项目演变过来）所以kernel代码是c，但并不多，外层都是c++。

79. 关于特化

todo 

```
/src/server/WFHttpServer.h
template<>
inline WFHttpServer::WFServer(http_process_t proc) :
	WFServerBase(&HTTP_SERVER_PARAMS_DEFAULT),
	process(std::move(proc))
{
}
```

因为server是用户构造的，所有用户拿到的类型都是一个类型，所以这里用了特化，而大家拿到的都是一个WFServer

只有一层不同没有必要行为派生

client的派生要复杂得多，拿到的是个client task，但new出来是个复合任务，没法通过简单的特化来做

server的行为足够简单，而client不可能通过特化来实现，因为派生层级不止一层

80. 如果我不采用websocket，而是服务端通过http chunk建立一个持久连接  有新消息时就推送，是不是效果也一样？而且chunk中途部分只传输长度和内容  似乎消耗更少

todo 

workflow框架默认的网络模式是一来一回的，也就是说推送过来之后client需要给server发东西、server才能继续发。另外断了是否重连如果这种模式，需要你自行解决。

websocket的协议里，基本也就是长度和内容，没有什么区别吧

wf中websocket是第一个非一来一回的协议

81. 希望能出一个基于tcp协议的非一来一回的方式，这样才能方便做到client端只请求一次，然后server端就一直往client端推送数据

https://github.com/holmes1412/workflow-major/blob/channel/tutorial/tutorial-10-user_defined_protocol/channel_client.cc

自定义tcp协议、双工client的例子

你需要使用websocket分支，因为网络框架目前在这个分支上。然后这个自定义协议里的协议就是原tutorial10里的那个，是可以和原server互通的

82. timer task 被进程退出打断时，其所在series的callback会被调起吗？

1) timerfd本身是可以被中断的，从epoll删掉就行；

2) series callback会被调起，会拿到一个ABORTED；

timer 如果开始计时，不能dissmiss了，但如果程序退出，不会卡住，会正常退出

83. 这里 timer 退出的时候为什么需要加这个锁呀？

https://github.com/sogou/workflow/issues/528

84. kernel中list -- 内核实现

todo 

这个list的好处是可以把一个数据结构既加入list也加入rbtree～内部超时有这种用法。好用就沿用了这些结构了

85. 关于大量使用裸指针

https://github.com/sogou/workflow/issues/29

项目用到现代C++的地方少，必须掌握也就function和move。

最大的遗憾还是11没有any，有几处用户接口用了void *，导致和现代c++的结合不太自然。后面我们再做上面的生态项目的话，代码风格会现代一些。

86. How to get multi-part form file from the http request?

Content-Type用来指定资源类型，multipart/form-data专用有效的传输文件

todo : demo

https://github.com/sogou/workflow/issues/28

87. 深入谈wf任务

todo: 

关于自定义协议的client/server，简单的就像turtorial-10那么实现就可以了。默计包含了DNS和retry功能。

而功能更强的client/server实现可以非常非常复杂。

http任务里，会自动补全header，会自动计算连接保持时间，client会redirect，server会自动识别连接保证次数，精确处理Expect: 100-continue请求。

redis任务里，包含了自动登陆的交互过程，数据库id的选取。

mysql任务，包含了复杂的登陆过程，对server发来的挑战数计算，字符集和数据库选取，更有复杂的事务状态处理，并且一切都是全异步的。mysql任务的实现几乎挑战了我们任务交互功能的极限。

kafka任务是一种典型的分布式任务，交互上主要是各种meta信息的更新，路由，rebalance等。kafka协议虽然复杂，但对我们框架来讲更为友好，因为我们天生适合分布式系统的实现。

所有协议不支持pipeline server，可以实现pipeline client，但有损系统美感，目前没有提供相关实现。之后打算直接上streaming引擎。

88. 关于引发惊群

todo : 

https://github.com/sogou/workflow/issues/38

89. Everything is non-blocking, please make sure your main process doesn’t terminate unexpectedly.

https://github.com/sogou/workflow/issues/76

这里就是上面说的wait_group

90. 在server的process函数里关停server的方法(Shutdown server in server’s process function) 

https://github.com/sogou/workflow/issues/89

code : [code](./demos/02_stop)

91. 关于请求限制

https://github.com/sogou/workflow/issues/135

code : [code](./demos/08_max_con)

92. 上传 / 下载文件

todo 

自己分块，在callback里发起下一个task

比较合理的一个做法就是约定好一个协议，有状态表示"未完成", 让我在callback里继续拿，比如http206之类的

93. WFCounterTask的一个作用是延长series使用

https://github.com/sogou/workflow/issues/301

series如果执行完、没有任务了就会结束，所以可以使用WFCounterTask作为内存开关，series内没有任务的时候放一个WFCounterTask

然后每个想往series里放的task，放入的时候都配合打开一下开关；

```
series->push_back(my_task);
WFTaskFactory::count_by_name("COUNTER_A");
```

就可以做到task本身被顺序执行，又能长期使用同一个series的做法了

94. 条件任务与资源池

https://github.com/sogou/workflow/blob/master/docs/about-conditional.md

95. file server

96. 如何汇总一个ParallelWork的结果

https://github.com/sogou/workflow/issues/140

[code](./demos/19_parallel/get_all_para.cc)

97. HttpMessage 中append_output_body_nocopy 在什么时候释放内存？

建议在callback里。如果是server task，可以设一个callback

98. task之间如何顺序传递数据？

https://github.com/sogou/workflow/issues/157

99. 服务端的process最后回复主调方的时机怎么理解的

https://github.com/sogou/workflow/issues/159

server task所在的series，是以processor为首任务，server task为末尾任务的。用户向这个series里添加任务，都不会影响到末尾的server task。

所以，当所有用户添加的任务都执行完成，server task被启动，而server task的启动行为就是reply，于是消息被回复。

100. HttpResponse类的append_output_body()函数是多线程安全的吗

https://github.com/sogou/workflow/issues/160

不是的！

不要在每个series或task的callback里，去append回复的resp。一定是在parellel的callback里汇总做这个事情。

你参考一下parallel_wget示例里的数据转递方法。

HttpMessage的所有操作都是单线程的，多线程操作没有什么意义。


101. 利用 calltree.pl 阅读代码

https://zhuanlan.zhihu.com/p/339910341


102. 如何保证用户请求严格按照先后顺序被处理和返回结果 

https://github.com/sogou/workflow/issues/559

一切串行化的需求都可以用resource pool来实现。

demo : 

https://github.com/chanchann/workflow_annotation/blob/main/demos/26_resource_pool/26_issue_559_server.cc

https://github.com/chanchann/workflow_annotation/blob/main/demos/26_resource_pool/26_issue_559_client.cc

103. server端如何使用WFGraphNode

https://github.com/sogou/workflow/issues/607

104. terminal下看代码

https://blog.csdn.net/guyongqiangx/article/details/70161189

105. keepalive、idle状态对应alive_list、idle_list处理机制

https://github.com/sogou/workflow/issues/202

Q1 : 当CommConnEntry处于CONN_STATE_KEEPALIVE状态时，add entry alive_list；

当CommConnEntry处于CONN_STATE_IDLE状态时，add entry idle_list;

alive_list与idle_list有何区别？

CONN_STATE_KEEPALIVE状态与CONN_STATE_IDLE状态有何区别？

alive_list与idle_list释放的entry时机在什么情况下发生？

A1 : 

idle_list本来是所有的client connection处于keep_alive状态时用的，idle_list里的所有连接下一个动作一定是send。

后来发现server处于准备回复状态的连接也很类似。

所以对于server的target来讲，idle_list其实最多只有一个连接，并且处于收到请求但还没有回复的阶段。

alive_list是service上的成员，保存该serivce上所有keep alive的连接。

这个list唯一的作用是drain，就是当连接数达到上限时用于关掉比较久没有使用的连接，以及程序退出的时候关闭所有keep alive连接。

Q2 : ref的主要功能是什么那？CommService中ref与CommConnEntry中ref区别？(entry->ref handle前加1，handle后减1)

A2 : ref就是引用计数啊，service需要引用计数到0才解绑完成，connection要ref=0才能释放。

因为异步环境下，连接随时可能被关闭，所有需要引用计数，相当于手动shared_ptr。

Q3 : 以下宏中CONN_STATE_RECEIVING的含义是？(不知为何没有CONN_STATE_SEND状态);

```
#define CONN_STATE_CONNECTING 0
#define CONN_STATE_CONNECTED 1
#define CONN_STATE_RECEIVING 2
#define CONN_STATE_SUCCESS 3
#define CONN_STATE_IDLE 4
#define CONN_STATE_KEEPALIVE 5
#define CONN_STATE_CLOSING 6
#define CONN_STATE_ERROR 7
```

A3 : 3、好像SENDING状态没有什么用，就没加。receiving就是正在收数据……

106. 关于WORKFLOW同一个进程内开多个HTTPSERVER的问题

https://github.com/sogou/workflow/issues/660

[详细解析](./demos/32_cpu_server)

107. Workflow遇到DNS解析出多个IP的处理

https://github.com/sogou/workflow/issues/659

todo 源码分析

108. 通过workflow实现转发功能的问题 

场景：A服务器通过代理服务器发消息给服务器B，B服务器需要根据A的IP来判断是否可以访问。

只有B服务器有服务器白名单IP的列表。代理服务器怎么使用workflow实现，来让B能识别A的ip。

https://github.com/sogou/workflow/issues/658

在proxy的process里，把A的地址拿出来，放到http header里转给B：

```cpp
void proxy_process(WFHttpTask *task)
{
    struct sockaddr_storage ss;
    socklen_t addrlen = sizeof ss;
    if (task->get_peer_addr(&ss, &addrlen) == 0)
    {
        //add a header;
    }
}
```

讨论未完待续...

109. 如何问问题

提issue的时候，最好直接说明你的实际需求，而不是说你试图解决的方案

https://xyproblem.info/

110. 如何优雅停止workflow创建的线程 

https://github.com/sogou/workflow/issues/654

如果你想提前关闭通信线程，在所有通信任务结束之后调用：

```cpp
#include "workflow/WFGlobal.h"
void my_close_scheduler()
{
    WFGlobal::get_scheduler()->deinit();
}
```

如果之后又想用通信任务的话，需要先重新初始化一下：

```cpp
int my_open_scheduler()
{
    const struct WFGlobalSettings *settings = WFGlobal::get_global_settings();
    return WFGlobal::get_scheduler()->init(settings->poller_threads, settings->handler_threads);
}
```

注意此处有一个小坑，因为程序退出会调用deinit。所以，如果你自己deinit过，程序退出之前最好重新init回来，可以调：WFGlobal::get_scheduler()->init(1, 1);

111. 如果task的callback还没有调用 就需要退出程序 如何处理比较合适

https://github.com/sogou/workflow/issues/654

先说结论 : 我们的网络任务没有callback不能结束程序

原因是：在很多情况下，你看到的网络任务并不是一个原子任务，而是可能包含多个异步过程

以http为例，可能需要dns，302重定向，重试等。每个过程结束了，不会判断scheduler是否已经被deinit

但如果你确定一个任务是原子任务，那么程序退出并不会有任何问题，行为有严格定义。也就是说，以下程序是绝对安全的

```cpp
void callback(WFHttpTask *task)
{
    // 这里打印的结果大概率是2，WFT_STATE_ABORTED。
    printf("state = %d\n", task->get_state());
}
int main()
{
    WFHttpTask *task = WFTaskFactory::create_http_task("https://127.0.0.1/", 0, 0, callback);
    task->start();
    // 这里直接结束程序
    return 1;
}
```

所以你只要确定你的任务没有重定向，重试，使用IP或域名dns信息肯定能cache命里，那么可以安全的结束程序，也可以随时调用WFGlobal::get_scheduler()->deinit()。

定时器任务也是一种原子任务，所以以下程序是安全的：

```cpp
void callback(WFTimerTask *task)
{
    // 这里打印的结果肯定是2，WFT_STATE_ABORTED。
    printf("state = %d\n", task->get_state());
}
int main()
{
    WFTimerTask *task = WFTaskFactory::create_timer_task(1000000, callback);
    task->start();
    // 这里直接结束程序
    return 1;
}
```

112. TCP server 如何主动发数据给到客户端

https://github.com/sogou/workflow/issues/649

利用push接口

113. MySQL Access Denied

workflow tutorial 里面mysql_cli : error msg: MySQL Access Denied

直接mysql -u -p可以登陆。server版本 8.0.26

A : mysql8的默认认证方式变了，需要你先改server一个配置。

你可以看看这个https://github.com/sogou/workflow/issues/186

114. 使用redis/MySQL client时无需先建立连接
首先看一下redis client任务的创建接口：

```cpp
class WFTaskFactory
{
public:
    WFRedisTask *create_redis_task(const std::string& url, int retry_max, redis_callback_t callback);
}
```

其中url的格式为：redis://:password@host:port/dbnum。port默认值为6379，dbnum默认值为0。

workflow的一个重要特点是由框架来管理连接，使用户接口可以极致的精简，并实现最有效的连接复用。

框架根据任务的用户名密码以及dbnum，来查找一个可以复用的连接。如果找不到则发起新连接并进行用户登陆，数据库选择等操作。

如果是一个新的host，还要进行DNS解析。请求出错还可能retry。这每一个步骤都是异步并且透明的，用户只需要填写自己的request，将任务启动，就可以在callback里得到请求的结果。

唯一需要注意的是，每次任务的创建都需要带着password，因为可能随时有登陆的需要。

同样的方法我们可以用来创建mysql任务。

但对于有事务需求的mysql，则需要通过我们的WFMySQLConnection来创建任务了，否则无法保证整个事务都在同一个连接上进行。WFMySQLConnection依然能做到连接和认证过程的异步性。

115. mysql客户端连接地址的密码中也包含字符@的问题

encode一下。

```cpp
{
    std::string url = "mysql://xxxx:" + StringUtil::url_encode_component("@@@123") + "@localhost/"
    WFMySQLTask *task = WFTaskFactory::create_mysql_task(url, ....);
}
```

https://github.com/sogou/workflow/issues/537

116. Mysql转义问题

在使用Mysql的set_query()方法时，发现插入字符串未经过转义的问题，请问workflow有实现EscapeString方法吗？

A : mysql的set_query()目前没有提供转义功能

https://github.com/sogou/workflow/issues/643

117. Mysql连接数过大问题

```cpp
#include "workflow/WFResourcePool.h"
#include "workflow/WFTaskFactory.h"
#include "workflow/WFHtttpServer.h"
#include "workflow/MySQLResult.h"

WFResourcePool respool(50);  // 假设最大50个并发

void mysql_callback(WFMySQLTask *task)
{
    respool.post(NULL);  // 归还资源
    ...
}

void process(WFHttpTask *server_task)
{
    WFMySQLTask *mysql_task = WFTaskFactory::create_mysql_task(..., mysql_callback);
    WFConditional *cond = respool.get(mysql_task);
    series_of(server_task)->push_back(cond);
}

int main()
{
    WFHttpServer(process);
    ....
}
```

1、产生mysql_task之后，通过respool.get得到一个条件任务。用条件任务代替mysql_task。

2、mysql_callback里，先通过respool.post归还资源。

https://github.com/sogou/workflow/issues/643

118. WFMySQLConnection

创建一个WFMySQLConnection的时候需要传入一个id，必须全局唯一，之后的调用内部都会由这个id去唯一找到对应的那个连接。

初始化需要传入url，之后在这个connection上创建的任务就不需要再设置url了。

https://github.com/sogou/workflow/issues/444




