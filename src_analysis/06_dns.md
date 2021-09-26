## dns

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