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

22. 
