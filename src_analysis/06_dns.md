# workflow源码解析10 : dns 01

项目源码 : https://github.com/sogou/workflow

更加详细的源码注释可看 : https://github.com/chanchann/workflow_annotation

协议rfc : https://datatracker.ietf.org/doc/html/rfc1035

我们使用getaddrinfo函数时，可以使用域名，内部自动处理了域名的请求，这个函数虽然方便，但它是阻塞的，不大适用自己写的高并发服务器。

我们可以先看create_dns_task

perl calltree.pl "create_dns_task" "" 1 1 3

```
create_dns_task
├── WFResolverTask::dispatch  [vim src/nameservice/WFDnsResolver.cc +271]
│   ├── start [vim _include/workflow/Workflow.h +68]
│   ├── Workflow::start_series_work   [vim _include/workflow/Workflow.h +169]
│   ├── Workflow::start_series_work   [vim _include/workflow/Workflow.h +185]
│   ├── SubTask::subtask_done [vim src/kernel/SubTask.cc +21]
│   ├── ParallelTask::dispatch        [vim src/kernel/SubTask.cc +54]
│   ├── __WFConditional::dispatch     [vim src/factory/WFResourcePool.cc +44]
│   ├── start [vim src/factory/Workflow.h +68]
│   ├── Workflow::start_series_work   [vim src/factory/Workflow.h +172]
│   └── Workflow::start_series_work   [vim src/factory/Workflow.h +188]
├── WFDnsClient::create_dns_task      [vim src/client/WFDnsClient.cc +238]
│   ├── WFResolverTask::dispatch      [vim src/nameservice/WFDnsResolver.cc +271]
│   ├── WFDnsClient::create_dns_task  [vim src/client/WFDnsClient.cc +238]
│   └── WFTaskFactory::create_dns_task        [vim src/factory/DnsTaskImpl.cc +163]
└── WFTaskFactory::create_dns_task    [vim src/factory/DnsTaskImpl.cc +163]
    ├── WFResolverTask::dispatch      [vim src/nameservice/WFDnsResolver.cc +271]
    ├── WFDnsClient::create_dns_task  [vim src/client/WFDnsClient.cc +238]
    └── WFTaskFactory::create_dns_task        [vim src/factory/DnsTaskImpl.cc +163]
```

大概可以看出，主要是两个来源，一个是WFDnsClient，一个是WFResolverTask

我们先看看WFDnsClient

## WFDnsClient

```cpp
// src/manager/WFGlobal.h
class WFGlobal
{
  ...
	static class WFDnsClient *get_dns_client();
  ...
};

```

然后可以看到他有个dns manager单例

```cpp
// src/manager/WFGlobal.cc
WFDnsClient *WFGlobal::get_dns_client()
{
	return __DnsClientManager::get_instance()->get_dns_client();
}
```

可以看出在这个地方构造的时候，生成了dnsClient

## __DnsClientManager

```cpp
// src/manager/WFGlobal.cc
class __DnsClientManager
{
private:
	__DnsClientManager()
	{
		const char *path = WFGlobal::get_global_settings()->resolv_conf_path;

		client = NULL;
		if (path && path[0])
		{
			int ndots = 1;
			int attempts = 2;
			bool rotate = false;  
			std::string url;
			std::string search; 

			__parse_resolv_conf(path, url, search, &ndots, &attempts, &rotate);
			if (url.size() == 0)
				url = "8.8.8.8";

			client = new WFDnsClient;
			if (client->init(url, search, ndots, attempts, rotate) >= 0)
				return;

			delete client;
			client = NULL;
		}
	}

	WFDnsClient *client;
};
```

`__parse_resolv_conf` 这里主要就是处理 `/etc/resolv.conf` 中的内容了

## __parse_resolv_conf

```cpp
// src/manager/WFGlobal.cc
static int __parse_resolv_conf(const char *path,
							   std::string& url, std::string& search_list,
							   int *ndots, int *attempts, bool *rotate)
{
	size_t bufsize = 0;
	char *line = NULL;
	FILE *fp;
	int ret;

	fp = fopen(path, "r");
	if (!fp)
		return -1;

	while ((ret = getline(&line, &bufsize, fp)) > 0)
	{
		if (strncmp(line, "nameserver", 10) == 0)
			__split_merge_str(line + 10, true, url);
		else if (strncmp(line, "search", 6) == 0)
			__split_merge_str(line + 6, false, search_list);
		else if (strncmp(line, "options", 7) == 0)
			__set_options(line + 7, ndots, attempts, rotate);
	}

	ret = ferror(fp) ? -1 : 0;
	free(line);
	fclose(fp);
	return ret;
}
```

然后 `__split_merge_str` 和 `__set_options` 细节可以看源码里面的注释，把传入的这几个参数都填上conf里读出的值

然后我们最终是把conf文件的这些参数读出来初始化WFDnsClient

那么我们再回过头看一下WFDnsClient

## WFDnsClient

```cpp
/src/client/WFDnsClient.h
class WFDnsClient
{
public:
	WFDnsClient() : params(NULL) { }
	virtual ~WFDnsClient() { }

	int init(const std::string& url);
	int init(const std::string& url, const std::string& search_list,
			 int ndots, int attempts, bool rotate);
	void deinit();

	WFDnsTask *create_dns_task(const std::string& name,
							   dns_callback_t callback);

private:
	void *params;
	std::atomic<size_t> id;
};

```

其中最主要的就是init和create_dns_task

```cpp
/src/client/WFDnsClient.cc
int WFDnsClient::init(const std::string& url, const std::string& search_list,
					  int ndots, int attempts, bool rotate)
{
  ...
	hosts = StringUtil::split_filter_empty(url, ',');
	for (size_t i = 0; i < hosts.size(); i++)
	{
		host = hosts[i];
		if (strncasecmp(host.c_str(), "dns://", 6) != 0 &&
			strncasecmp(host.c_str(), "dnss://", 7) != 0)
		{
			host = "dns://" + host;
		}

		if (URIParser::parse(host, uri) != 0)
			return -1;

		uris.emplace_back(std::move(uri));
	}

	if (uris.empty() || ndots < 0 || attempts < 1)
	{
		errno = EINVAL;
		return -1;
	}

	this->params = new DnsParams;
	DnsParams::dns_params *q = ((DnsParams *)(this->params))->get_params();
	q->uris = std::move(uris);
	q->search_list = StringUtil::split_filter_empty(search_list, ',');
	q->ndots = ndots > 15 ? 15 : ndots;
	q->attempts = attempts > 5 ? 5 : attempts;
	q->rotate = rotate;

	return 0;
}
```

这一步主要是做了两件事，解析我们的host，设置dns的 params 

```cpp
/src/client/WFDnsClient.cc
struct dns_params
{
  std::vector<ParsedURI> uris;
  std::vector<std::string> search_list;
  int ndots;
  int attempts;
  bool rotate;
};
```

第二个重要的是

```cpp
/src/client/WFDnsClient.cc
WFDnsTask *WFDnsClient::create_dns_task(const std::string& name,
										dns_callback_t callback)
{
	DnsParams::dns_params *p = ((DnsParams *)(this->params))->get_params();
	struct DnsStatus status;
	size_t next_server;
	WFDnsTask *task;
	DnsRequest *req;

	next_server = p->rotate ? this->id++ % p->uris.size() : 0;

	status.origin_name = name;
	status.next_domain = 0;
	status.attempts_left = p->attempts;
	status.try_origin_state = DNS_STATUS_TRY_ORIGIN_FIRST;

	if (!name.empty() && name.back() == '.')
		status.next_domain = p->uris.size();
	else if (__get_ndots(name) < p->ndots)
		status.try_origin_state = DNS_STATUS_TRY_ORIGIN_LAST;

	__has_next_name(p, &status);

	task = WFTaskFactory::create_dns_task(p->uris[next_server],
										  0, std::move(callback));
	status.next_server = next_server;
	status.last_server = (next_server + p->uris.size() - 1) % p->uris.size();

	req = task->get_req();
	req->set_question(status.current_name.c_str(), DNS_TYPE_A, DNS_CLASS_IN);
	req->set_rd(1);

	ComplexTask *ctask = static_cast<ComplexTask *>(task);
	*ctask->get_mutable_ctx() = std::bind(__callback_internal,
										  std::placeholders::_1,
										  *(DnsParams *)params, status);

	return task;
}

```

可以看到其中还有个DnsStatus, 我们看看他的内部结构

```cpp
/src/client/WFDnsClient.cc
struct DnsStatus
{
	std::string origin_name;
	std::string current_name;
	size_t next_server;			// next server to try
	size_t last_server;			// last server to try
	size_t next_domain;			// next search domain to try
	int attempts_left;
	int try_origin_state;
};
```

在它内部还是用的工厂创建的task

```cpp
/src/factory/DnsTaskImpl.cc
WFDnsTask *WFTaskFactory::create_dns_task(const ParsedURI& uri,
										  int retry_max,
										  dns_callback_t callback)
{
	ComplexDnsTask *task = new ComplexDnsTask(retry_max, std::move(callback));
	const char *name;
  
	if (uri.path && uri.path[0] && uri.path[1])
		name = uri.path + 1;
	else  
		name = ".";

	DnsRequest *req = task->get_req();
	req->set_question(name, DNS_TYPE_A, DNS_CLASS_IN);

	task->init(uri);
	task->set_keep_alive(DNS_KEEPALIVE_DEFAULT);
	return task;
}
```

## ComplexDnsTask

我们可以看到`create_dns_task`实际上是new了一个`ComplexDnsTask`

```cpp
class ComplexDnsTask : public WFComplexClientTask<DnsRequest, DnsResponse,
							  std::function<void (WFDnsTask *)>>
{
	static struct addrinfo hints;

public:
	ComplexDnsTask(int retry_max, dns_callback_t&& cb):
		WFComplexClientTask(retry_max, std::move(cb))
	{
		// dns 是建立在 UDP 之上的应用层协议
		// WFComplexClientTask中默认TT_TCP
		this->set_transport_type(TT_UDP);
	}

protected:
	virtual CommMessageOut *message_out();
	virtual CommMessageIn *message_in();
	virtual bool init_success();
	virtual bool finish_once();

private:
	bool need_redirect();
};
```

继承自`WFComplexClientTask`，构造的时候就是设置`TT_UDP`



