#! https://zhuanlan.zhihu.com/p/448005710
# workflow 源码解析 : http server 01

项目源码 : https://github.com/sogou/workflow

更加详细的源码注释可看 : https://github.com/chanchann/workflow_annotation

我们先看最简单的http例子

https://github.com/chanchann/workflow_annotation/blob/main/demos/07_http/http_no_reply.cc

## 流程分析

## 启动 WFServerBase::start

当我们构造除了一个server，然后start

```cpp
int WFServerBase::start(int family, const char *host, unsigned short port,
						const char *cert_file, const char *key_file)
{
	struct addrinfo hints = {
		.ai_flags		=	AI_PASSIVE,    // key
		.ai_family		=	family,
		.ai_socktype	=	SOCK_STREAM,
	};
	struct addrinfo *addrinfo;
	char port_str[PORT_STR_MAX + 1];
	int ret;
    ...
	snprintf(port_str, PORT_STR_MAX + 1, "%d", port);
	getaddrinfo(host, port_str, &hints, &addrinfo);
    start(addrinfo->ai_addr, (socklen_t)addrinfo->ai_addrlen,
                cert_file, key_file);
    freeaddrinfo(addrinfo);
    ...
}
```

```
// https://man7.org/linux/man-pages/man3/getaddrinfo.3.html

当host = NULL 传进去时

If the AI_PASSIVE flag is specified in hints.ai_flags, and node
is NULL, then the returned socket addresses will be suitable for
bind(2)ing a socket that will accept(2) connections. 

The returned socket address will contain the "wildcard address"
(INADDR_ANY for IPv4 addresses, IN6ADDR_ANY_INIT for IPv6
address).  

The wildcard address is used by applications
(typically servers) that intend to accept connections on any of
the host's network addresses.  

If node is not NULL, then the AI_PASSIVE flag is ignored.
```

这正start的是

```cpp

int ::start(const struct sockaddr *bind_addr, socklen_t addrlen,
						const char *cert_file, const char *key_file)
{
    ...
	this->init(bind_addr, addrlen, cert_file, key_file);
    this->scheduler->bind(this);
    ...
}
```

这里init了WFServerBase

然后bind，进入了一个server的基本流程

### bind

```
note:

https://man7.org/linux/man-pages/man2/bind.2.html

When a socket is created with socket(2), it exists in a name
space (address family) but has no address assigned to it.  

bind() assigns the address specified by addr to the socket referred to
by the file descriptor sockfd.

It is normally necessary to assign a local address using bind()
before a SOCK_STREAM socket may receive connections (see
accept(2)).
```

```cpp
class CommScheduler
{
    ...
	int bind(CommService *service)
	{
		return this->comm.bind(service);
	}
    ...
private:
	Communicator comm;
};
```

```cpp
// 这里的CommService 是为了产生listenfd
int Communicator::bind(CommService *service)
{
	struct poller_data data;

	sockfd = this->nonblock_listen(service);

    service->listen_fd = sockfd;
    service->ref = 1;
    data.operation = PD_OP_LISTEN;
    data.fd = sockfd;
    data.accept = Communicator::accept;
    data.context = service;
    data.result = NULL;
    mpoller_add(&data, service->listen_timeout, this->mpoller);
}
```

这里就可见，bind并不是单纯的bind，把listen操作都打包一起了

## bind + listen

```cpp
int Communicator::nonblock_listen(CommService *service)
{
	int sockfd = service->create_listen_fd();

    __set_fd_nonblock(sockfd)

    __bind_and_listen(sockfd, service->bind_addr,
                            service->addrlen);
}
```

```cpp

static int __bind_and_listen(int sockfd, const struct sockaddr *addr,
							 socklen_t addrlen)
{
    ...
	bind(sockfd, addr, addrlen);
    ...
	return listen(sockfd, SOMAXCONN);
}
```

然后把listen操作加入epoll中监听

## epoll检测到listen时

```cpp
static void *__poller_thread_routine(void *arg)
{
    ...
    case PD_OP_LISTEN:
    __poller_handle_listen(node, poller);
    break;
    ...
}
```

```cpp
static void __poller_handle_listen(struct __poller_node *node,
								   poller_t *poller)
{
    ...
	while (1)
	{
        ...
        // 1. 这里调用了accept建立连接
		sockfd = accept(node->data.fd, (struct sockaddr *)&ss, &len);

        // data.accept = Communicator::accept;
        // 2. 调用Communicator::accept，初始化
		p = node->data.accept((const struct sockaddr *)&ss, len,
							  sockfd, node->data.context);
		res->data = node->data;
		res->data.result = p;
		res->error = 0;
		res->state = PR_ST_SUCCESS;
        // .callback			=	Communicator::callback,
        /*
            void Communicator::callback(struct poller_result *res, void *context)
            {
                Communicator *comm = (Communicator *)context;
                msgqueue_put(res, comm->queue);
            }
        */
        // 放回结果到msgqueue中
		poller->cb((struct poller_result *)res, poller->ctx);

		res = (struct __poller_node *)malloc(sizeof (struct __poller_node));
		node->res = res;
		if (!res)
			break;
	}

	if (__poller_remove_node(node, poller))
		return;
    ...
}
```

## Communicator::handler_thread_routine

当我们检测到listen，然后将epoll中listen事件带的callback执行，然后将结果写道msgqueue中

然后到了消费者流程，在handler_thread_routine中，拿到这个结果

```cpp

void Communicator::handler_thread_routine(void *context)
{
    case PD_OP_LISTEN:
        comm->handle_listen_result(res);
        break;
}
```

```cpp
void Communicator::handle_listen_result(struct poller_result *res)
{
	CommService *service = (CommService *)res->data.context;

	...
	case PR_ST_SUCCESS:
		target = (CommServiceTarget *)res->data.result;
		entry = this->accept_conn(target, service);

		res->data.operation = PD_OP_READ;
		res->data.message = NULL;
		timeout = target->response_timeout;
		...

		if (res->data.operation != PD_OP_LISTEN)
		{
			res->data.fd = entry->sockfd;
			res->data.ssl = entry->ssl;
			res->data.context = entry;
			if (mpoller_add(&res->data, timeout, this->mpoller) >= 0)
			{
				if (this->stop_flag)
					mpoller_del(res->data.fd, this->mpoller);
				break;
			}
		}
		...
}
```

简化下流程就是产生`CommConnEntry`， 然把read事件放进epoll进行监听，因为建立连接，就要等着对方发消息了

### Communicator::accept_conn

```cpp
struct CommConnEntry *Communicator::accept_conn(CommServiceTarget *target,
												CommService *service)
{
	__set_fd_nonblock(target->sockfd);

	size = offsetof(struct CommConnEntry, mutex);
	entry = (struct CommConnEntry *)malloc(size);

	entry->conn = service->new_connection(target->sockfd);

	entry->seq = 0;
	entry->mpoller = this->mpoller;
	entry->service = service;
	entry->target = target;
	entry->ssl = NULL;
	entry->sockfd = target->sockfd;
	entry->state = CONN_STATE_CONNECTED;
	entry->ref = 1;
}
```

这里就是产生 `CommConnEntry` 把这些信息保存下来

### Communicator::accept

我们可以看出，监听的data中，

cb是`Communicator::accept`

context是 `service`

```cpp
void *Communicator::accept(const struct sockaddr *addr, socklen_t addrlen,
						   int sockfd, void *context)
{
	CommService *service = (CommService *)context;
	CommServiceTarget *target = new CommServiceTarget;
    ...
    target->init(addr, addrlen, 0, service->response_timeout);
    service->incref();
    target->service = service;
    target->sockfd = sockfd;
    target->ref = 1;
    ...
}
```

accpet 就是初始化CommServiceTarget

而这个`CommServiceTarget`是`CommTarget`的子类

所以调用的是

```cpp

int CommTarget::init(const struct sockaddr *addr, socklen_t addrlen,
					 int connect_timeout, int response_timeout)
{
    ...
	this->addr = (struct sockaddr *)malloc(addrlen);

    ret = pthread_mutex_init(&this->mutex, NULL);

    memcpy(this->addr, addr, addrlen);
    this->addrlen = addrlen;
    this->connect_timeout = connect_timeout;
    this->response_timeout = response_timeout;
    INIT_LIST_HEAD(&this->idle_list);

    this->ssl_ctx = NULL;
    this->ssl_connect_timeout = 0;
    ...
}
```

### WFServerBase::init

```cpp
int WFServerBase::init(const struct sockaddr *bind_addr, socklen_t addrlen,
					   const char *cert_file, const char *key_file)
{
    ...
	this->CommService::init(bind_addr, addrlen, -1, timeout);
	this->scheduler = WFGlobal::get_scheduler();
    ...
}
```

这里主要是调用了父类的init

### CommService::init

```cpp
int CommService::init(const struct sockaddr *bind_addr, socklen_t addrlen,
					  int listen_timeout, int response_timeout)
{
	this->bind_addr = (struct sockaddr *)malloc(addrlen);
    pthread_mutex_init(&this->mutex, NULL);
    memcpy(this->bind_addr, bind_addr, addrlen);
    this->addrlen = addrlen;
    this->listen_timeout = listen_timeout;
    this->response_timeout = response_timeout;
    INIT_LIST_HEAD(&this->alive_list);
    this->ssl_ctx = NULL;
    this->ssl_accept_timeout = 0;
}
```

就是设置好这些参数


