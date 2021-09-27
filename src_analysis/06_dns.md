## dns

注释部分都在workflow文件夹中的源码中

协议rfc : https://datatracker.ietf.org/doc/html/rfc1035

我们使用getaddrinfo函数时，可以使用域名，内部自动处理了域名的请求，这个函数虽然方便，但它是阻塞的，不大适用自己写的高并发服务器。

perl calltree.pl "(?)dns" "" 1 1 2

```
 (?)dns
  ├── WFGlobal::get_dns_cache
  │   ├── WFGlobal::get_dns_cache       [vim src/manager/WFGlobal.cc +728]
  │   ├── WFResolverTask::dispatch      [vim src/nameservice/WFDnsResolver.cc +271]
  │   └── WFResolverTask::dns_callback_internal [vim src/nameservice/WFDnsResolver.cc +451]
  ├── WFGlobal::get_dns_client
  │   ├── WFGlobal::get_dns_client      [vim src/manager/WFGlobal.cc +718]
  │   └── WFResolverTask::dispatch      [vim src/nameservice/WFDnsResolver.cc +271]
  ├── WFGlobal::get_dns_executor
  │   ├── get_dns_queue [vim src/manager/WFGlobal.cc +305]
  │   ├── get_dns_executor      [vim src/manager/WFGlobal.cc +374]
  │   ├── WFGlobal::get_dns_executor    [vim src/manager/WFGlobal.cc +768]
  │   └── __create_thread_dns_task      [vim src/nameservice/WFDnsResolver.cc +213]
  ├── WFGlobal::get_dns_queue
  │   ├── get_dns_queue [vim src/manager/WFGlobal.cc +369]
  │   ├── WFGlobal::get_dns_queue       [vim src/manager/WFGlobal.cc +763]
  │   └── __create_thread_dns_task      [vim src/nameservice/WFDnsResolver.cc +213]
  ├── WFGlobal::get_dns_resolver
  │   ├── WFGlobal::get_dns_resolver    [vim src/manager/WFGlobal.cc +778]
  │   ├── WFServiceGovernance::create_router_task       [vim src/nameservice/WFServiceGovernance.cc +121]
  │   ├── WFResolverTask::dispatch      [vim src/nameservice/WFDnsResolver.cc +271]
  │   ├── WFResolverTask::dns_single_callback   [vim src/nameservice/WFDnsResolver.cc +493]
  │   └── WFResolverTask::dns_partial_callback  [vim src/nameservice/WFDnsResolver.cc +517]
  ├── WFTaskFactory::create_dns_task
  │   ├── WFResolverTask::dispatch      [vim src/nameservice/WFDnsResolver.cc +271]
  │   ├── WFDnsClient::create_dns_task  [vim src/client/WFDnsClient.cc +238]
  │   └── WFTaskFactory::create_dns_task        [vim src/factory/DnsTaskImpl.cc +163]
  ├── __create_thread_dns_task
  │   └── WFResolverTask::dispatch      [vim src/nameservice/WFDnsResolver.cc +271]
  ├── __dns_parser_free_record
  │   ├── __dns_parser_free_record_list [vim src/protocol/dns_parser.c +161]
  │   └── __dns_parser_parse_record     [vim src/protocol/dns_parser.c +539]
  ├── __dns_parser_free_record_list
  │   └── dns_parser_deinit     [vim src/protocol/dns_parser.c +861]
  ├── __dns_parser_parse_a
  │   └── __dns_parser_parse_record     [vim src/protocol/dns_parser.c +539]
  ├── __dns_parser_parse_aaaa
  │   └── __dns_parser_parse_record     [vim src/protocol/dns_parser.c +539]
  ├── __dns_parser_parse_host
  │   ├── __dns_parser_parse_names      [vim src/protocol/dns_parser.c +245]
  │   ├── __dns_parser_parse_soa        [vim src/protocol/dns_parser.c +313]
  │   ├── __dns_parser_parse_srv        [vim src/protocol/dns_parser.c +388]
  │   ├── __dns_parser_parse_mx [vim src/protocol/dns_parser.c +445]
  │   ├── __dns_parser_parse_record     [vim src/protocol/dns_parser.c +539]
  │   └── __dns_parser_parse_question   [vim src/protocol/dns_parser.c +658]
  ├── __dns_parser_parse_mx
  │   └── __dns_parser_parse_record     [vim src/protocol/dns_parser.c +539]
  ├── __dns_parser_parse_names
  │   └── __dns_parser_parse_record     [vim src/protocol/dns_parser.c +539]
  ├── __dns_parser_parse_others
  │   └── __dns_parser_parse_record     [vim src/protocol/dns_parser.c +539]
  ├── __dns_parser_parse_question
  │   └── dns_parser_parse_all  [vim src/protocol/dns_parser.c +762]
  ├── __dns_parser_parse_record
  │   └── dns_parser_parse_all  [vim src/protocol/dns_parser.c +762]
  ├── __dns_parser_parse_soa
  │   └── __dns_parser_parse_record     [vim src/protocol/dns_parser.c +539]
  ├── __dns_parser_parse_srv
  │   └── __dns_parser_parse_record     [vim src/protocol/dns_parser.c +539]
  ├── __dns_parser_uint16
  │   ├── __dns_parser_parse_host       [vim src/protocol/dns_parser.c +65]
  │   ├── __dns_parser_parse_srv        [vim src/protocol/dns_parser.c +388]
  │   ├── __dns_parser_parse_mx [vim src/protocol/dns_parser.c +445]
  │   ├── __dns_parser_parse_record     [vim src/protocol/dns_parser.c +539]
  │   ├── __dns_parser_parse_question   [vim src/protocol/dns_parser.c +658]
  │   └── dns_parser_append_message     [vim src/protocol/dns_parser.c +797]
  ├── __dns_parser_uint32
  │   ├── __dns_parser_parse_soa        [vim src/protocol/dns_parser.c +313]
  │   └── __dns_parser_parse_record     [vim src/protocol/dns_parser.c +539]
  ├── __dns_parser_uint8
  │   └── __dns_parser_parse_host       [vim src/protocol/dns_parser.c +65]
  ├── create_dns_task
  │   ├── WFResolverTask::dispatch      [vim src/nameservice/WFDnsResolver.cc +271]
  │   ├── WFDnsClient::create_dns_task  [vim src/client/WFDnsClient.cc +238]
  │   └── WFTaskFactory::create_dns_task        [vim src/factory/DnsTaskImpl.cc +163]
  ├── dns_additional_cursor_init
  │   ├── reset_additional_cursor       [vim _include/workflow/DnsUtil.h +63]
  │   └── reset_additional_cursor       [vim src/protocol/DnsUtil.h +63]
  ├── dns_answer_cursor_init
  │   ├── DnsResultCursor       [vim _include/workflow/DnsUtil.h +41]
  │   ├── reset_answer_cursor   [vim _include/workflow/DnsUtil.h +53]
  │   ├── DnsResultCursor       [vim src/protocol/DnsUtil.h +41]
  │   └── reset_answer_cursor   [vim src/protocol/DnsUtil.h +53]
  ├── dns_authority_cursor_init
  │   ├── reset_authority_cursor        [vim _include/workflow/DnsUtil.h +58]
  │   └── reset_authority_cursor        [vim src/protocol/DnsUtil.h +58]
  ├── dns_callback_internal
  │   ├── WFResolverTask::dispatch      [vim src/nameservice/WFDnsResolver.cc +271]
  │   ├── WFResolverTask::dns_single_callback   [vim src/nameservice/WFDnsResolver.cc +493]
  │   ├── WFResolverTask::dns_parallel_callback [vim src/nameservice/WFDnsResolver.cc +537]
  │   └── WFResolverTask::thread_dns_callback   [vim src/nameservice/WFDnsResolver.cc +580]
  ├── dns_flag_
  │   └── __CommManager [vim src/manager/WFGlobal.cc +380]
  ├── dns_manager_
  │   └── __CommManager [vim src/manager/WFGlobal.cc +380]
  ├── dns_params
  │   └── DnsParams     [vim src/client/WFDnsClient.cc +44]
  ├── dns_parser_append_message
  │   └── DnsMessage::append    [vim src/protocol/DnsMessage.cc +157]
  ├── dns_parser_deinit
  │   ├── ~DnsMessage   [vim _include/workflow/DnsMessage.h +174]
  │   ├── ~DnsMessage   [vim src/protocol/DnsMessage.h +174]
  │   ├── operator =    [vim src/protocol/DnsMessage.cc +51]
  │   └── DnsResponse::append   [vim src/protocol/DnsMessage.cc +181]
  ├── dns_parser_init
  │   ├── DnsMessage    [vim _include/workflow/DnsMessage.h +167]
  │   ├── DnsMessage    [vim src/protocol/DnsMessage.h +167]
  │   └── DnsResponse::append   [vim src/protocol/DnsMessage.cc +181]
  ├── dns_parser_parse_all
  │   └── dns_parser_append_message     [vim src/protocol/dns_parser.c +797]
  ├── dns_parser_set_id
  │   ├── DnsRequest    [vim _include/workflow/DnsMessage.h +203]
  │   └── DnsRequest    [vim src/protocol/DnsMessage.h +203]
  ├── dns_parser_set_question
  │   ├── set_question  [vim _include/workflow/DnsMessage.h +211]
  │   └── set_question  [vim src/protocol/DnsMessage.h +211]
  ├── dns_parser_set_question_name
  │   └── dns_parser_set_question       [vim src/protocol/dns_parser.c +716]
  ├── dns_record_cursor_find_cname
  │   ├── find_cname    [vim _include/workflow/DnsUtil.h +79]
  │   └── find_cname    [vim src/protocol/DnsUtil.h +79]
  ├── dns_record_cursor_next
  │   ├── next  [vim _include/workflow/DnsUtil.h +68]
  │   └── next  [vim src/protocol/DnsUtil.h +68]
  ├── get_dns_cache
  │   ├── WFGlobal::get_dns_cache       [vim src/manager/WFGlobal.cc +728]
  │   ├── WFResolverTask::dispatch      [vim src/nameservice/WFDnsResolver.cc +271]
  │   └── WFResolverTask::dns_callback_internal [vim src/nameservice/WFDnsResolver.cc +451]
  ├── get_dns_client
  │   ├── WFGlobal::get_dns_client      [vim src/manager/WFGlobal.cc +718]
  │   └── WFResolverTask::dispatch      [vim src/nameservice/WFDnsResolver.cc +271]
  ├── get_dns_executor
  │   ├── get_dns_queue [vim src/manager/WFGlobal.cc +305]
  │   ├── get_dns_executor      [vim src/manager/WFGlobal.cc +374]
  │   ├── WFGlobal::get_dns_executor    [vim src/manager/WFGlobal.cc +768]
  │   └── __create_thread_dns_task      [vim src/nameservice/WFDnsResolver.cc +213]
  ├── get_dns_manager_safe
  │   ├── get_dns_queue [vim src/manager/WFGlobal.cc +369]
  │   └── get_dns_executor      [vim src/manager/WFGlobal.cc +374]
  ├── get_dns_queue
  │   ├── get_dns_queue [vim src/manager/WFGlobal.cc +369]
  │   ├── WFGlobal::get_dns_queue       [vim src/manager/WFGlobal.cc +763]
  │   └── __create_thread_dns_task      [vim src/nameservice/WFDnsResolver.cc +213]
  └── get_dns_resolver
      ├── WFGlobal::get_dns_resolver    [vim src/manager/WFGlobal.cc +778]
      ├── WFServiceGovernance::create_router_task       [vim src/nameservice/WFServiceGovernance.cc +121]
      ├── WFResolverTask::dispatch      [vim src/nameservice/WFDnsResolver.cc +271]
      ├── WFResolverTask::dns_single_callback   [vim src/nameservice/WFDnsResolver.cc +493]
      └── WFResolverTask::dns_partial_callback  [vim src/nameservice/WFDnsResolver.cc +517]
  
```

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

__parse_resolv_conf这里主要就是处理/etc/resolv.conf 中的内容了

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

然后__split_merge_str和__set_options细节可以看源码里面的注释，把传入的这几个参数都填上conf里读出的值

然后我们最终是把conf文件的这些参数读出来初始化WFDnsClient

那么我们看一下WFDnsClient

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
	std::vector<std::string> hosts;
	std::vector<ParsedURI> uris;
	std::string host;
	ParsedURI uri;

	id = 0;
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

todo

## dns protocol

![dns protocol](./pics/dns_protocol.png)

DnsRequest和DnsResponse 继承自 DnsMessage

我们的request就是设置好question