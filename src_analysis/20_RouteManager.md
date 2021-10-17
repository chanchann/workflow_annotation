#! https://zhuanlan.zhihu.com/p/422001127
# workflow源码解析13 : RouteManager

router是负责dns解析或upstream查找的

```cpp
class RouteManager
{
public:
	class RouteResult {...}
	class RouteTarget : public CommSchedTarget {...}

public:
	int get(TransportType type,
			const struct addrinfo *addrinfo,
			const std::string& other_info,
			const struct EndpointParams *endpoint_params,
			const std::string& hostname,
			RouteResult& result);

	RouteManager()
	{
		cache_.rb_node = NULL;
	}

	~RouteManager();

private:
	std::mutex mutex_;
	struct rb_root cache_;

public:
	static void notify_unavailable(void *cookie, CommTarget *target);
	static void notify_available(void *cookie, CommTarget *target);
};

```

这里可以看出几个关键点

他有两个inner class

## RouteResult

这个是需要返回给上层用户的结果

其中重要的包含了个`CommSchedObject *request_object;`, 这个我们后面会知道什么用

```cpp
class RouteResult
{
public:
	void *cookie;
	CommSchedObject *request_object;

public:
	RouteResult(): cookie(NULL), request_object(NULL) { }
	void clear() { cookie = NULL; request_object = NULL; }
};
```

这个我们很熟悉，在

```cpp
template<class REQ, class RESP, typename CTX>
void WFComplexClientTask<REQ, RESP, CTX>::dispatch()
{
	...
		if (this->route_result_.request_object)   
	...
```

出现过，这个就是我们最终发要的出来的`route_result_.request_object`

## RouteTarget

```cpp
class RouteTarget : public CommSchedTarget
{
public:
	int state;

private:
	virtual WFConnection *new_connection(int connect_fd)
	{
		return new WFConnection;
	}

public:
	RouteTarget() : state(0) { }
};

```

这个继承自CommSchedTarget，一看就是给底层通信用的

其中

```cpp

using RouteTargetTCP = RouteManager::RouteTarget;

class RouteTargetUDP : public RouteManager::RouteTarget
{
private:
	virtual int create_connect_fd()
	{
		const struct sockaddr *addr;
		socklen_t addrlen;

		this->get_addr(&addr, &addrlen);
		return socket(addr->sa_family, SOCK_DGRAM, 0);
	}
};
```

我们这些不同的target，需要去实现不同协议的create_connect_fd。

最终

```cpp
int Communicator::nonblock_connect(CommTarget *target)
{
	int sockfd = target->create_connect_fd();
	...
}
```

实现不同的target的create_connect_fd

## RouteManager::get

其中最为重要的部分是

```cpp
int RouteManager::get(TransportType type,
					  const struct addrinfo *addrinfo,
					  const std::string& other_info,
					  const struct EndpointParams *endpoint_params,
					  const std::string& hostname,
					  RouteResult& result)
{
	...
	uint64_t md5_16 = __generate_key(type, addrinfo, other_info,
									 endpoint_params, hostname);
	...


	entry = new RouteResultEntry;
	entry->init(&params);

	result.cookie = entry;
	result.request_object = entry->request_object;  // 在这里就把request_object拿出来了
	...
}
```

## RouteResultEntry::init

```cpp
int RouteResultEntry::init(const struct RouteParams *params)
{
	const struct addrinfo *addr = params->addrinfo;
	CommSchedTarget *target;

	if (addr->ai_next == NULL) // 只有一个
	{
		target = this->create_target(params, addr);

		// std::vector<CommSchedTarget *> targets;

		this->targets.push_back(target);
		this->request_object = target;   // 这里就把创建的target拿给用户了
		this->md5_16 = params->md5_16; 
	}

	this->group = new CommSchedGroup();
	if (this->group->init() >= 0)
	{
		if (this->add_group_targets(params) >= 0)
		{
			this->request_object = this->group;
			this->md5_16 = params->md5_16;
			return 0;
		}

		this->group->deinit();
	}

	delete this->group;
	return -1;
}
```

我们在这就产生了request_object

然后最终拿出给用户。

## RouteResultEntry::create_target

```cpp

CommSchedTarget *RouteResultEntry::create_target(const struct RouteParams *params,
												 const struct addrinfo *addr)
{
	CommSchedTarget *target;

	switch (params->transport_type)
	{
	...
	case TT_TCP:
			target = new RouteTargetTCP();
		break;
	case TT_UDP:
		target = new RouteTargetUDP();
		break;
	...

	target->init(addr->ai_addr, addr->ai_addrlen, params->ssl_ctx,
					 params->connect_timeout, params->ssl_connect_timeout,
					 params->response_timeout, params->max_connections);

}
```

就是new相应协议的target

## 总结

我们的routeManager，在我们的dns任务中，通过cache或者获取到的addrinfo

我们通过addrinfo 可以得出target，这个target和RouteResult的request_object的公共祖先是CommSchedObject

我们最后获取的是RouteResult.request_object就是这个协议相应的target，用来创造connect_fd的。

## todo

这里简单梳理了机制相关，关于一些策略细节，留到以后再看