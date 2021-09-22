## 通过calltree.pl 查找结构 

calltree.pl '(?i)epoll' '' 1  1  4

```
(?i)epoll
  ├── epoll_create
  │   └── __poller_create_pfd   [vim src/kernel/poller.c +93]
  │       └── poller_create     [vim src/kernel/poller.c +1056]
  │           └── __mpoller_create      [vim src/kernel/mpoller.c +24]
  ├── epoll_ctl
  │   ├── __poller_add_fd       [vim src/kernel/poller.c +98]
  │   │   ├── __poller_open_pipe        [vim src/kernel/poller.c +1018]
  │   │   │   └── poller_start  [vim src/kernel/poller.c +1119]
  │   │   └── poller_add        [vim src/kernel/poller.c +1223]
  │   │       ├── mpoller_add   [vim _include/workflow/mpoller.h +48]
  │   │       └── mpoller_add   [vim src/kernel/mpoller.h +48]
  │   ├── __poller_del_fd       [vim src/kernel/poller.c +110]
  │   │   ├── __poller_remove_node      [vim src/kernel/poller.c +302]
  │   │   │   ├── __poller_handle_read  [vim src/kernel/poller.c +403]
  │   │   │   ├── __poller_handle_write [vim src/kernel/poller.c +477]
  │   │   │   ├── __poller_handle_listen        [vim src/kernel/poller.c +564]
  │   │   │   ├── __poller_handle_connect       [vim src/kernel/poller.c +611]
  │   │   │   ├── __poller_handle_ssl_accept    [vim src/kernel/poller.c +637]
  │   │   │   ├── __poller_handle_ssl_connect   [vim src/kernel/poller.c +665]
  │   │   │   ├── __poller_handle_ssl_shutdown  [vim src/kernel/poller.c +693]
  │   │   │   ├── __poller_handle_event [vim src/kernel/poller.c +721]
  │   │   │   └── __poller_handle_notify        [vim src/kernel/poller.c +780]
  │   │   ├── __poller_handle_timeout   [vim src/kernel/poller.c +857]
  │   │   │   └── __poller_thread_routine       [vim src/kernel/poller.c +943]
  │   │   ├── poller_del        [vim src/kernel/poller.c +1288]
  │   │   │   ├── mpoller_del   [vim _include/workflow/mpoller.h +55]
  │   │   │   └── mpoller_del   [vim src/kernel/mpoller.h +55]
  │   │   └── poller_stop       [vim src/kernel/poller.c +1488]
  │   │       ├── mpoller_start [vim src/kernel/mpoller.c +67]
  │   │       └── mpoller_stop  [vim src/kernel/mpoller.c +86]
  │   ├── __poller_mod_fd       [vim src/kernel/poller.c +115]
  │   │   ├── __poller_handle_ssl_error [vim src/kernel/poller.c +366]
  │   │   │   ├── __poller_handle_read  [vim src/kernel/poller.c +403]
  │   │   │   ├── __poller_handle_write [vim src/kernel/poller.c +477]
  │   │   │   ├── __poller_handle_ssl_accept    [vim src/kernel/poller.c +637]
  │   │   │   ├── __poller_handle_ssl_connect   [vim src/kernel/poller.c +665]
  │   │   │   └── __poller_handle_ssl_shutdown  [vim src/kernel/poller.c +693]
  │   │   └── poller_mod        [vim src/kernel/poller.c +1331]
  │   │       ├── mpoller_mod   [vim _include/workflow/mpoller.h +61]
  │   │       └── mpoller_mod   [vim src/kernel/mpoller.h +61]
  │   └── __poller_add_timerfd  [vim src/kernel/poller.c +133]
  │       └── __poller_create_timer     [vim src/kernel/poller.c +1038]
  │           └── poller_create [vim src/kernel/poller.c +1056]
  └── epoll_wait
      └── __poller_wait [vim src/kernel/poller.c +156]
          └── __poller_thread_routine   [vim src/kernel/poller.c +943]
```

封装基本的三个操作，epoll_create， epoll_ctl， epoll_wait

## epoll_create

calltree.pl 'poller_create' '' 1  1  6

```
poller_create
└── __mpoller_create  [vim src/kernel/mpoller.c +24]
    └── mpoller_create        [vim src/kernel/mpoller.c +45]
        └── Communicator::create_poller       [vim src/kernel/Communicator.cc +1313]
            └── Communicator::init    [vim src/kernel/Communicator.cc +1341]
                ├── main      [vim tutorial/tutorial-13-kafka_cli.cc +182]
                ├── ParsedURI [vim _include/workflow/URIParser.h +51]
                ├── init      [vim _include/workflow/CommScheduler.h +57]
                ├── init      [vim _include/workflow/CommScheduler.h +114]
                ├── ParsedURI [vim src/util/URIParser.h +51]
                ├── ParsedURI::__copy [vim src/util/URIParser.cc +324]
                ├── ParsedURI::__move [vim src/util/URIParser.cc +397]
                ├── WFServerBase::init        [vim src/server/WFServer.cc +88]
                ├── WFServerBase::start       [vim src/server/WFServer.cc +166]
                ├── __DnsManager      [vim src/manager/WFGlobal.cc +308]
                ├── get_scheduler     [vim src/manager/WFGlobal.cc +343]
                ├── __CommManager     [vim src/manager/WFGlobal.cc +380]
                ├── get_exec_queue    [vim src/manager/WFGlobal.cc +467]
                ├── __ExecManager     [vim src/manager/WFGlobal.cc +509]
                ├── __DnsClientManager        [vim src/manager/WFGlobal.cc +680]
                ├── RouteResultEntry::create_target   [vim src/manager/RouteManager.cc +162]
                ├── RouteResultEntry::init    [vim src/manager/RouteManager.cc +199]
                ├── RouteManager::get [vim src/manager/RouteManager.cc +437]
                ├── UPSVNSWRRPolicy::add_server_locked        [vim src/nameservice/UpstreamPolicies.cc +651]
                ├── UPSVNSWRRPolicy::remove_server_locked     [vim src/nameservice/UpstreamPolicies.cc +658]
                ├── WFKafkaClient::init       [vim src/client/WFKafkaClient.cc +1545]
                ├── WFDnsClient::init [vim src/client/WFDnsClient.cc +184]
                ├── MySQLResultCursor::MySQLResultCursor      [vim src/protocol/MySQLResult.cc +64]
                ├── MySQLResultCursor::MySQLResultCursor      [vim src/protocol/MySQLResult.cc +79]
                ├── MySQLResultCursor::reset  [vim src/protocol/MySQLResult.cc +84]
                ├── Communicator::accept      [vim src/kernel/Communicator.cc +1262]
                ├── init      [vim src/kernel/CommScheduler.h +57]
                ├── init      [vim src/kernel/CommScheduler.h +114]
                ├── CommSchedTarget::init     [vim src/kernel/CommScheduler.cc +47]
                ├── WFTaskFactory::create_http_task   [vim src/factory/HttpTaskImpl.cc +760]
                ├── WFTaskFactory::create_http_task   [vim src/factory/HttpTaskImpl.cc +776]
                ├── WFTaskFactory::create_http_task   [vim src/factory/HttpTaskImpl.cc +790]
                ├── WFTaskFactory::create_http_task   [vim src/factory/HttpTaskImpl.cc +810]
                ├── WFTaskFactory::create_mysql_task  [vim src/factory/MySQLTaskImpl.cc +679]
                ├── WFTaskFactory::create_mysql_task  [vim src/factory/MySQLTaskImpl.cc +696]
                ├── WFTaskFactory::create_dns_task    [vim src/factory/DnsTaskImpl.cc +173]
                ├── WFTaskFactory::create_redis_task  [vim src/factory/RedisTaskImpl.cc +265]
                ├── WFTaskFactory::create_redis_task  [vim src/factory/RedisTaskImpl.cc +278]
                ├── __ComplexKafkaTask::message_out   [vim src/factory/KafkaTaskImpl.cc +112]
                ├── __ComplexKafkaTask::check_redirect        [vim src/factory/KafkaTaskImpl.cc +321]
                ├── __WFKafkaTaskFactory::create_kafka_task   [vim src/factory/KafkaTaskImpl.cc +580]
                ├── __WFKafkaTaskFactory::create_kafka_task   [vim src/factory/KafkaTaskImpl.cc +593]
                ├── __WFKafkaTaskFactory::create_kafka_task   [vim src/factory/KafkaTaskImpl.cc +604]
                └── __WFKafkaTaskFactory::create_kafka_task   [vim src/factory/KafkaTaskImpl.cc +617]
```

可见在 Communicator::init 中创建

```cpp
int Communicator::init(size_t poller_threads, size_t handler_threads)
{
	if (poller_threads == 0)
	{
		errno = EINVAL; // EINVAL表示无效的参数，即为invalid argument 
		return -1;
	}

	if (this->create_poller(poller_threads) >= 0)
	{
		if (this->create_handler_threads(handler_threads) >= 0)
		{
			this->stop_flag = 0;
			return 0;   // init成功
		}
		// 没create成功则
		mpoller_stop(this->mpoller);
		mpoller_destroy(this->mpoller);
		msgqueue_destroy(this->queue);
	}

	return -1;
}
```

calltree.pl 'Communicator::create_poller' '' 0 1 2

```
Communicator::create_poller   [vim src/kernel/Communicator.cc +1313]
├── msgqueue_create   [vim src/kernel/msgqueue.c +122]
│   ├── pthread_mutex_init
│   ├── pthread_cond_init
│   ├── pthread_cond_destroy
│   └── pthread_mutex_destroy
├── mpoller_create    [vim src/kernel/mpoller.c +45]
│   ├── offsetof
│   └── __mpoller_create      [vim src/kernel/mpoller.c +24]
│       ├── poller_create     [vim src/kernel/poller.c +1056]
│       └── poller_destroy    [vim src/kernel/poller.c +1110]
├── mpoller_start     [vim src/kernel/mpoller.c +67]
│   ├── poller_start  [vim src/kernel/poller.c +1119]
│   │   ├── pthread_mutex_lock
│   │   ├── __poller_open_pipe        [vim src/kernel/poller.c +1018]
│   │   ├── pthread_create
│   │   ├── close
│   │   └── pthread_mutex_unlock
│   └── poller_stop   [vim src/kernel/poller.c +1488]
│       ├── write
│       ├── pthread_join
│       ├── pthread_mutex_lock
│       ├── __poller_handle_pipe      [vim src/kernel/poller.c +835]
│       ├── close
│       ├── rb_entry
│       ├── rb_erase  [vim src/kernel/rbtree.c +222]
│       ├── + list_add
│       ├── + list_splice_init
│       ├── list_for_each_safe
│       ├── list_entry
│       ├── + list_del
│       ├── + __poller_del_fd
│       └── pthread_mutex_unlock
├── mpoller_destroy   [vim src/kernel/mpoller.c +94]
│   └── poller_destroy        [vim src/kernel/poller.c +1110]
│       ├── pthread_mutex_destroy
│       └── close
└── msgqueue_destroy  [vim src/kernel/msgqueue.c +168]
    ├── pthread_cond_destroy
    └── pthread_mutex_destroy
```

可见create_poller 完成这几件事 : msgqueue_create, mpoller_create

继续深挖poller

```cpp
mpoller_t *mpoller_create(const struct poller_params *params, size_t nthreads)
{
	mpoller_t *mpoller;
	size_t size;

	if (nthreads == 0)
		nthreads = 1;

	size = offsetof(mpoller_t, poller) + nthreads * sizeof (void *);
	mpoller = (mpoller_t *)malloc(size);
	if (mpoller)
	{
		mpoller->nthreads = (unsigned int)nthreads;
		if (__mpoller_create(params, mpoller) >= 0)
			return mpoller;

		free(mpoller);
	}

	return NULL;
}
```



## msgqueue

perl calltree.pl '(?i)msgqueue' '' 1 1 2

```
(?i)msgqueue
├── __msgqueue_swap
│   └── msgqueue_get  [vim src/kernel/msgqueue.c +102]
├── msgqueue_create
│   └── Communicator::create_poller   [vim src/kernel/Communicator.cc +1319]
├── msgqueue_destroy
│   ├── Communicator::create_poller   [vim src/kernel/Communicator.cc +1319]
│   ├── Communicator::init    [vim src/kernel/Communicator.cc +1356]
│   └── Communicator::deinit  [vim src/kernel/Communicator.cc +1380]
├── msgqueue_get
│   └── Communicator::handler_thread_routine  [vim src/kernel/Communicator.cc +1083]
├── msgqueue_put
│   └── Communicator::callback        [vim src/kernel/Communicator.cc +1256]
└── msgqueue_set_nonblock
    ├── Communicator::create_handler_threads  [vim src/kernel/Communicator.cc +1286]
    └── Communicator::deinit  [vim src/kernel/Communicator.cc +1380]
```