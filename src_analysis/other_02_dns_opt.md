#! https://zhuanlan.zhihu.com/p/448013325
# workflow杂记02 : dns的优化

分析代码改动地址 : https://github.com/sogou/workflow/commit/0eed99b538d824579c3e339625a6727646dca343

项目源码 : https://github.com/sogou/workflow

更加详细的源码注释可看 : https://github.com/chanchann/workflow_annotation

## 新版代码

关于dns cache

https://github.com/chanchann/workflow_annotation/blob/main/src_analysis/other_01_cache_lock.md

已经浅谈过，就只有两个地方出现

一个是在`WFResolverTask::dispatch`中取，一个是在`WFResolverTask::dns_callback_internal`中放

```cpp
DNS_CACHE_LEVEL_0	->	NO cache
DNS_CACHE_LEVEL_1	->	TTL MIN
DNS_CACHE_LEVEL_2	->	TTL [DEFAULT]
DNS_CACHE_LEVEL_3	->	Forever
```

其中比较常用的是LEVEL_1 和 LEVEL_2

WFDnsResolver::create_router_task

```cpp
int dns_cache_level = params->retry_times == 0 ? DNS_CACHE_LEVEL_2 :
													DNS_CACHE_LEVEL_1;
```

```cpp
void WFResolverTask::dispatch()
{
	// part 1
	// 有dns cache
	if (dns_cache_level_ != DNS_CACHE_LEVEL_0)
	{
		DnsCache *dns_cache = WFGlobal::get_dns_cache();
		// using DnsHandle = LRUHandle<HostPort, DnsCacheValue>;
		const DnsCache::DnsHandle *addr_handle;

		switch (dns_cache_level_)
		{
		case DNS_CACHE_LEVEL_1:  // TTL MIN
			addr_handle = dns_cache->get_confident(host_, port_);
			break;

		case DNS_CACHE_LEVEL_2:  // TTL [DEFAULT]
			addr_handle = dns_cache->get_ttl(host_, port_);
			break;

		case DNS_CACHE_LEVEL_3:  // Forever
			addr_handle = dns_cache->get(host_, port_);
			break;

		default:
			addr_handle = NULL;
			break;
		}
		// 如果get到了cache而且没过期
		if (addr_handle)
		{
			// 单例
			// 用来解析dns的
			auto *route_manager = WFGlobal::get_route_manager();
			// 拿出cache中的 addrinfo
			struct addrinfo *addrinfo = addr_handle->value.addrinfo;
			struct addrinfo first;
			// (bool)first_addr_only_ = params->fixed_addr;
			// 如果我们只需要第一个，而且addrinfo后面还有，我们还要把后面砍断
			if (first_addr_only_ && addrinfo->ai_next)
			{
				first = *addrinfo;   // 这里拷贝了一次
				first.ai_next = NULL;
				addrinfo = &first;
			}

			// 这里是核心解析了，主要就是把route_result_.request_object get到
			if (route_manager->get(type_, addrinfo, info_, &endpoint_params_,
								   host_, this->result) < 0)
			{
				this->state = WFT_STATE_SYS_ERROR;
				this->error = errno;
			}
			else
				this->state = WFT_STATE_SUCCESS;

			// 然后就可以release了
			dns_cache->release(addr_handle);
			query_dns_ = false;  // 就直接不去查dns了
			this->subtask_done();
			return;
		}
	}


	// part2
	// 如果没有cache
	// 又还有host_
	// 比如www.baidu.com
	if (!host_.empty())
	{
		char front = host_.front();  
		char back = host_.back();
		struct in6_addr addr;
		int ret;
		
		// 假如是ip
		// ipv6 : 2001:0db8:02de:0000:0000:0000:0000:0e13
		if (host_.find(':') != std::string::npos)
			ret = inet_pton(AF_INET6, host_.c_str(), &addr);
		// ipv4 : 192.11.11.11  
		// 其中没有 ':' , 而且首位都是数字
		else if (isdigit(back) && isdigit(front))
			ret = inet_pton(AF_INET, host_.c_str(), &addr);
		else if (front == '/')    // 这个是local path的时候  // todo
			ret = 1;
		else
			ret = 0;

		if (ret == 1)
		{
			DnsInput dns_in;
			DnsOutput dns_out;

			dns_in.reset(host_, port_);
			DnsRoutine::run(&dns_in, &dns_out);  // 这里通过getaddrino 得到了addrinfo(在dns_out中)
			// 获得了才加上AI_PASSIVE
			// 为什么在之后再add_passive？
			__add_passive_flags((struct addrinfo *)dns_out.get_addrinfo());  
			dns_callback_internal(&dns_out, (unsigned int)-1, (unsigned int)-1);
			query_dns_ = false;   
			this->subtask_done();
			return;
		}
	}

	// part3
	// 如果没有cache
	// 还没有host_
	// for server?

	// "/etc/hosts"
	// 操作和上面基本相同
	const char *hosts = WFGlobal::get_global_settings()->hosts_path;
	if (hosts)
	{
		struct addrinfo *ai;
		int ret = __readaddrinfo(hosts, host_.c_str(), port_, &__ai_hints, &ai);

		if (ret == 0)
		{
			DnsOutput out;
			// 注意这里是create
			DnsRoutine::create(&out, ret, ai);
			__add_passive_flags((struct addrinfo *)out.get_addrinfo());
			dns_callback_internal(&out, dns_ttl_default_, dns_ttl_min_);
			query_dns_ = false;
			this->subtask_done();
			return;
		}
	}

	// 如果都没有"/etc/hosts"，那么我们就得
	WFDnsClient *client = WFGlobal::get_dns_client();
	if (client)
	{
		static int family = __default_family();
		WFDnsResolver *resolver = WFGlobal::get_dns_resolver();

		if (family == AF_INET || family == AF_INET6)
		{
			auto&& cb = std::bind(&WFResolverTask::dns_single_callback,
								  this,
								  std::placeholders::_1);
			WFDnsTask *dns_task = client->create_dns_task(host_, std::move(cb));

			if (family == AF_INET6)
				dns_task->get_req()->set_question_type(DNS_TYPE_AAAA);

			WFConditional *cond = resolver->respool.get(dns_task);
			series_of(this)->push_front(cond);
		}
		else
		{
			/*
				struct DnsContext
				{
					int state;
					int error;
					int eai_error;
					unsigned short port;
					struct addrinfo *ai;
				};
			*/
			struct DnsContext *dctx = new struct DnsContext[2];
			WFDnsTask *task_v4;
			WFDnsTask *task_v6;
			ParallelWork *pwork;

			dctx[0].ai = NULL;
			dctx[1].ai = NULL;
			dctx[0].port = port_;
			dctx[1].port = port_;

			task_v4 = client->create_dns_task(host_, dns_partial_callback);
			task_v4->user_data = dctx;

			task_v6 = client->create_dns_task(host_, dns_partial_callback);
			task_v6->get_req()->set_question_type(DNS_TYPE_AAAA);
			task_v6->user_data = dctx + 1;

			auto&& cb = std::bind(&WFResolverTask::dns_parallel_callback,
								  this,
								  std::placeholders::_1);

			pwork = Workflow::create_parallel_work(std::move(cb));
			pwork->set_context(dctx);

			WFConditional *cond_v4 = resolver->respool.get(task_v4);
			WFConditional *cond_v6 = resolver->respool.get(task_v6);
			pwork->add_series(Workflow::create_series_work(cond_v4, nullptr));
			pwork->add_series(Workflow::create_series_work(cond_v6, nullptr));

			series_of(this)->push_front(pwork);
		}
	}
	else
	{
		auto&& cb = std::bind(&WFResolverTask::thread_dns_callback,
							  this,
							  std::placeholders::_1);
		ThreadDnsTask *dns_task = __create_thread_dns_task(host_, port_,
														   std::move(cb));
		series_of(this)->push_front(dns_task);
	}

	query_dns_ = true;
	this->subtask_done();
}
```

# 这段代码重要部分拆分解析

## part1 
### DNS_CACHE_LEVEL_1 : dns_cache->get_confident

```cpp
const DnsHandle *get_confident(const HostPort& host_port)
{
	return get_inner(host_port, GET_TYPE_CONFIDENT);
}
```

### case DNS_CACHE_LEVEL_2 : dns_cache->get_ttl

```cpp
const DnsHandle *get_ttl(const HostPort& host_port)
{
	return get_inner(host_port, GET_TYPE_TTL);
}
```

### DnsCache::get_inner

其实这两个接口都是调用get_inner

不过传的type不一样罢了

这里在DnsCache解析时，详细探究，这里我们只用知道他得到了DnsHandle(using DnsHandle = LRUHandle<HostPort, DnsCacheValue>;) 即可

### route_manager

这里我们看RouteManager解析部分

https://github.com/chanchann/workflow_annotation/blob/main/src_analysis/20_RouteManager.md

## part2

### DnsRoutine

其中一个重要的点是`DnsRoutine`，详细见`DnsRoutine`的文章

## __add_passive_flags

```cpp
static void __add_passive_flags(struct addrinfo *ai)
{
	while (ai)
	{
		ai->ai_flags |= AI_PASSIVE;
		ai = ai->ai_next;
	}
}
```

通常服务器端在调用getaddrinfo之前，ai_flags设置AI_PASSIVE，用于bind；主机名nodename通常会设置为NULL，返回通配地址[::]。

## WFResolverTask::dns_callback_internal

就是把获取到的addrinfo 放到dns_cache 中，然后利用我们得到的addrinfo, 把WFResolverTask的父类的result获取到

```cpp
class WFRouterTask : public WFGenericTask
{
	...
	RouteManager::RouteResult result;
};
```


```cpp

void WFResolverTwask::dns_callback_internal(DnsOutput *dns_out,
										   unsigned int ttl_default,
										   unsigned int ttl_min)
{
	...
	struct addrinfo *addrinfo = dns_out->move_addrinfo();
	...
	addr_handle = dns_cache->put(host_, port_, addrinfo,
									(unsigned int)ttl_default,
									(unsigned int)ttl_min);
	route_manager->get(type_, addrinfo, info_, &endpoint_params_,
							host_, this->result);

	dns_cache->release(addr_handle);

}
```

## part3

### __readaddrinfo

这里基本和part2操作相同，有个最大的区别就在于__readaddrinfo

```cpp

static int __readaddrinfo(const char *path,
						  const char *name, unsigned short port,
						  const struct addrinfo *hints,
						  struct addrinfo **res)
{
	char port_str[PORT_STR_MAX + 1];
	size_t bufsize = 0;
	char *line = NULL;
	int count = 0;
	struct addrinfo h;
	int errno_bak;
	FILE *fp;
	int ret;

	fp = fopen(path, "r");
	if (!fp)
		return EAI_SYSTEM;

	h = *hints;
	h.ai_flags |= AI_NUMERICSERV | AI_NUMERICHOST,
	snprintf(port_str, PORT_STR_MAX + 1, "%u", port);

	errno_bak = errno;
	while ((ret = getline(&line, &bufsize, fp)) > 0)
	{
		if (__readaddrinfo_line(line, name, port_str, &h, res) == 0)
		{
			count++;
			res = &(*res)->ai_next;
		}
	}

	ret = ferror(fp) ? EAI_SYSTEM : EAI_NONAME;
	free(line);
	fclose(fp);
	if (count != 0)
	{
		errno = errno_bak;
		return 0;
	}

	return ret;
}
```

```cpp

// hosts line format: IP canonical_name [aliases...] [# Comment]
static int __readaddrinfo_line(char *p, const char *name, const char *port,
							   const struct addrinfo *hints,
							   struct addrinfo **res)
{
	const char *ip = NULL;
	char *start;

	start = p;
	while (*start != '\0' && *start != '#')
		start++;
	*start = '\0';

	while (1)
	{
		while (isspace(*p))
			p++;

		start = p;
		while (*p != '\0' && !isspace(*p))
			p++;

		if (start == p)
			break;

		if (*p != '\0')
			*p++ = '\0';

		if (ip == NULL)
		{
			ip = start;
			continue;
		}

		if (strcasecmp(name, start) == 0)
		{
			if (getaddrinfo(ip, port, hints, res) == 0)
				return 0;
		}
	}

	return 1;
}
```
