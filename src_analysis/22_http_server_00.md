#! https://zhuanlan.zhihu.com/p/448005560
# workflow 源码解析 : http server 00

项目源码 : https://github.com/sogou/workflow

更加详细的源码注释可看 : https://github.com/chanchann/workflow_annotation

我们先看最简单的http例子

https://github.com/chanchann/workflow_annotation/blob/main/demos/07_http/http_no_reply.cc

我们先看看server有什么基本的的部件

## WFHttpServer

```cpp
using WFHttpServer = WFServer<protocol::HttpRequest,
							  protocol::HttpResponse>;
```

实际上是WFServer的全特化版本(加了具体的协议)

```cpp

template<class REQ, class RESP>
class WFServer : public WFServerBase
{
public:
	WFServer(const struct WFServerParams *params,
			 std::function<void (WFNetworkTask<REQ, RESP> *)> proc) :
		WFServerBase(params),
		process(std::move(proc))
	{
	}

	WFServer(std::function<void (WFNetworkTask<REQ, RESP> *)> proc) :
		WFServerBase(&SERVER_PARAMS_DEFAULT),
		process(std::move(proc))
	{
	}

protected:
	virtual CommSession *new_session(long long seq, CommConnection *conn);

protected:
	std::function<void (WFNetworkTask<REQ, RESP> *)> process;
};
```

这里有四个重点

1. 其继承自`WFServerBase`

2. 这一层增加了process(server最为核心的逻辑接口)

3. params

4. new_session 产生一次交互，其实就是产生server_task

```
note : workflow 就是一来一回的方式

因为是异步框架，他干什么事情都把事情挂在epoll上监听，到事件来临，把结果写到res中，放到msgqueue中

然后让handler thread消费，看是哪种类型，产生相应的动作
```

### WFServerBase

```cpp
class WFServerBase : protected CommService
{
public:
	WFServerBase(const struct WFServerParams *params) :
		conn_count(0)
	{
		this->params = *params;
		this->listen_fd = -1;
		this->unbind_finish = false;
	}

public:
	int start(unsigned short port);   // 各种重载
	int serve(int listen_fd);
	void stop();
	void shutdown();
	void wait_finish();

public:
	size_t get_conn_count() const { return this->conn_count; }

protected:
	virtual SSL_CTX *get_server_ssl_ctx(const char *servername);

protected:
	WFServerParams params;

protected:
	virtual WFConnection *new_connection(int accept_fd);
	void delete_connection(WFConnection *conn);

private:
	int init(const struct sockaddr *bind_addr, socklen_t addrlen,
			 const char *cert_file, const char *key_file);
	int init_ssl_ctx(const char *cert_file, const char *key_file);
	static long ssl_ctx_callback(SSL *ssl, int *al, void *arg);
	virtual int create_listen_fd();
	virtual void handle_unbound();

protected:
	std::atomic<size_t> conn_count;

private:
	int listen_fd;
	bool unbind_finish;

	std::mutex mutex;
	std::condition_variable cond;

	CommScheduler *scheduler;
};
```

这一层就是服务器应该有的各种功能，启动，停止，创建listenfd，创建/删除连接

具体这些功能怎么用，到调用的时候再看

还有一个需要注意的是 `WFServerBase` 继承自 `CommService`

`CommService`  用来产生listenfd, 产生新的连接

### new_session

```cpp
template<class REQ, class RESP>
CommSession *WFServer<REQ, RESP>::new_session(long long seq, CommConnection *conn)
{
	using factory = WFNetworkTaskFactory<REQ, RESP>;
	WFNetworkTask<REQ, RESP> *task;

	task = factory::create_server_task(this, this->process);
	task->set_keep_alive(this->params.keep_alive_timeout);
	task->set_receive_timeout(this->params.receive_timeout);
	task->get_req()->set_size_limit(this->params.request_size_limit);

	return task;
}
```

这里的 `CommSession` 就是 `server_task`, 并且设置好各种参数

CommSession是一次req->resp的交互，主要要实现message_in(), message_out()等几个虚函数，让核心知道怎么产生消息。

对server来讲，session是被动产生的(后面来看看如何产生)

### WFNetworkTaskFactory

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

	static T *create_client_task(TransportType type,
								 const std::string& url,
								 int retry_max,
								 std::function<void (T *)> callback);

	static T *create_client_task(TransportType type,
								 const ParsedURI& uri,
								 int retry_max,
								 std::function<void (T *)> callback);

public:
	static T *create_server_task(CommService *service,
								 std::function<void (T *)>& process);
};
```

### create_server_task

```cpp
template<class REQ, class RESP>
WFNetworkTask<REQ, RESP> *
WFNetworkTaskFactory<REQ, RESP>::create_server_task(CommService *service,
				std::function<void (WFNetworkTask<REQ, RESP> *)>& process)
{
	return new WFServerTask<REQ, RESP>(service, WFGlobal::get_scheduler(),
									   process);
}
```

实际上是new了`WFServerTask`

### WFServerTask

```cpp
template<class REQ, class RESP>
class WFServerTask : public WFNetworkTask<REQ, RESP>
{
protected:
	virtual CommMessageOut *message_out() { return &this->resp; }
	virtual CommMessageIn *message_in() { return &this->req; }
	virtual void handle(int state, int error);

protected:
	virtual void dispatch();
	virtual WFConnection *get_connection() const;
protected:
	CommService *service;

protected:
	class Processor : public SubTask
	{
	public:
		Processor(WFServerTask<REQ, RESP> *task,
				  std::function<void (WFNetworkTask<REQ, RESP> *)>& proc);

		virtual void dispatch();

		virtual SubTask *done();

		std::function<void (WFNetworkTask<REQ, RESP> *)>& process;
		WFServerTask<REQ, RESP> *task;
	} processor;

	class Series : public SeriesWork
	{
	public:
		Series(WFServerTask<REQ, RESP> *task);
		virtual ~Series();
		CommService *service;
	};

public:
	WFServerTask(CommService *service, CommScheduler *scheduler,
				 std::function<void (WFNetworkTask<REQ, RESP> *)>& proc);
protected:
	virtual ~WFServerTask() { }
};
```

1. 首先每一个task都是一次交互，所以我们得实现`message_out`, `message_in`, `handle`

2. 这里还有两个内部类，`Processor`, `Series`, 这个我们在分析流程的时候看看，到底有何用

