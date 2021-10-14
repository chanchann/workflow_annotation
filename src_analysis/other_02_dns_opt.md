
# workflow杂记02 : 分析一次cache中lock的改动

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

```cpp
void WFResolverTask::dispatch()
{
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
		// 如果有cache，直接return
		if (addr_handle)
		{
			auto *route_manager = WFGlobal::get_route_manager();
			struct addrinfo *addrinfo = addr_handle->value.addrinfo;
			struct addrinfo first;

			if (first_addr_only_ && addrinfo->ai_next)
			{
				first = *addrinfo;
				first.ai_next = NULL;
				addrinfo = &first;
			}

			if (route_manager->get(type_, addrinfo, info_, &endpoint_params_,
								   host_, this->result) < 0)
			{
				this->state = WFT_STATE_SYS_ERROR;
				this->error = errno;
			}
			else
				this->state = WFT_STATE_SUCCESS;

			dns_cache->release(addr_handle);
			query_dns_ = false;  // 就直接不去查dns了
			this->subtask_done();
			return;
		}
	}

	if (!host_.empty())
	{
		char front = host_.front();
		char back = host_.back();
		struct in6_addr addr;
		int ret;

		if (host_.find(':') != std::string::npos)
			ret = inet_pton(AF_INET6, host_.c_str(), &addr);
		else if (isdigit(back) && isdigit(front))
			ret = inet_pton(AF_INET, host_.c_str(), &addr);
		else if (front == '/')
			ret = 1;
		else
			ret = 0;

		if (ret == 1)
		{
			DnsInput dns_in;
			DnsOutput dns_out;

			dns_in.reset(host_, port_);
			DnsRoutine::run(&dns_in, &dns_out);
			__add_passive_flags((struct addrinfo *)dns_out.get_addrinfo());
			dns_callback_internal(&dns_out, (unsigned int)-1, (unsigned int)-1);
			query_dns_ = false;
			this->subtask_done();
			return;
		}
	}

	const char *hosts = WFGlobal::get_global_settings()->hosts_path;
	if (hosts)
	{
		struct addrinfo *ai;
		int ret = __readaddrinfo(hosts, host_.c_str(), port_, &__ai_hints, &ai);

		if (ret == 0)
		{
			DnsOutput out;
			DnsRoutine::create(&out, ret, ai);
			__add_passive_flags((struct addrinfo *)out.get_addrinfo());
			dns_callback_internal(&out, dns_ttl_default_, dns_ttl_min_);
			query_dns_ = false;
			this->subtask_done();
			return;
		}
	}

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