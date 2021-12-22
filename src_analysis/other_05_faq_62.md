#! https://zhuanlan.zhihu.com/p/448014305
# workflow杂记05 : server task在什么时候销毁

可见 : faq 62 : 看了下proxy的教程，原始的task在series没有结束的时候是不会被销毁的，对吗?

项目源码 : https://github.com/sogou/workflow

更加详细的源码注释可看 : https://github.com/chanchann/workflow_annotation

### task分为两种情况来分析看看，一种client的情况，一种server的情况

client task的“执行”，指的是“发送request - 收回response”；

server task的“执行”，指的是“收到request - 回复response”；

### client的情况 :

如果task没执行，那就是还在series的队列里呢，不会销毁. 

如果task执行完了就销毁了

举个例子，01-wget中，我们client产生一个http task

```cpp
WFHttpTask *task = WFTaskFactory::create_http_task(url, REDIRECT_MAX, RETRY_MAX,
										   wget_callback);

...
task->start();
```

这里的task实际上是new了一个`ComplexHttpTask`, 他调用start

```cpp
void start()
{
    assert(!series_of(this));
    Workflow::start_series_work(this, nullptr);
}
```

实际上要先启动一个series，

```cpp
inline void
Workflow::start_series_work(SubTask *first, SubTask *last,
							series_callback_t callback)
{
	SeriesWork *series = new SeriesWork(first, std::move(callback));
	series->set_last_task(last);
	first->dispatch();
}
```

在dipatch后，进入subtask_done, 执行done

```cpp
void CommRequest::dispatch()
{
	if (this->scheduler->request(this, this->object, this->wait_timeout,
								 &this->target) < 0)
	{
		this->state = CS_STATE_ERROR;
		this->error = errno;
		if (errno != ETIMEDOUT)
			this->timeout_reason = TOR_NOT_TIMEOUT;
		else
			this->timeout_reason = TOR_WAIT_TIMEOUT;

		this->subtask_done();
	}
}
```

```cpp
// WFNetworkTask
virtual SubTask *done()
{
    SeriesWork *series = series_of(this);

    if (this->state == WFT_STATE_SYS_ERROR && this->error < 0)
    {
        this->state = WFT_STATE_SSL_ERROR;
        this->error = -this->error;
    }

    if (this->callback)
        this->callback(this);

    delete this;
    return series->pop();
}
```

执行完task里面的callback之后，就delete销毁掉了

2) server的task 

而proxy的例子是个server的task，就以此为例

首先，我们要知道在哪里产生server task

```cpp
// WFHttpServer.h

template<>
inline CommSession *WFHttpServer::new_session(long long seq, CommConnection *conn)
{
	WFHttpTask *task;

	task = WFServerTaskFactory::create_http_task(this, this->process);
	task->set_keep_alive(this->params.keep_alive_timeout);
	task->set_receive_timeout(this->params.receive_timeout);
	task->get_req()->set_size_limit(this->params.request_size_limit);

	return task;
}
```

在这里产生，而又在哪里调用的这个函数呢?

```cpp
int Communicator::create_service_session(struct CommConnEntry *entry)
{
    ...
	session = service->new_session(entry->seq, entry->conn);
    ...
}
```

```cpp
poller_message_t *Communicator::create_message(void *context)
{
    ...
	if (entry->state == CONN_STATE_CONNECTED ||
		entry->state == CONN_STATE_KEEPALIVE)
	{
		if (Communicator::create_service_session(entry) < 0)
			return NULL;
	}
    ...
}
```

而这里到了我们熟悉的poller的阶段了，server task的执行指的是“收到request - 回复response”，我们收到了request消息，会new_session产生一个WFServerTask

这里如何交互可以看http_server的代码解析

[http_server_00](22_http_server_00.md)

[http_server_01](22_http_server_01.md)

[http_server_02](22_http_server_02.md)

server“回复response”的时机是series里没有东西了再回复（毕竟我们要实现异步server）

因为我们把WFServerTask设置为last，等到最后执行这个task dispatch的时候reply

```cpp
virtual void dispatch()
{
	if (this->state == WFT_STATE_TOREPLY)
	{
		/* Enable get_connection() again if the reply() call is success. */
		this->processor.task = this;
		if (this->scheduler->reply(this) >= 0)
			return;
		...
	}

	this->subtask_done();
}
```

具体分析可见 [http_server_02](22_http_server_02.md)

注意这个说法 : server task的生命会持续到series结束，本质是在于它还没执行完回复。

这个说法错误，可见 https://github.com/chanchann/workflow_annotation/issues/1

他在这里dispatch 完 依旧是 走到 `WFNetworkTask::done`

```cpp
virtual SubTask *done()
{
	SeriesWork *series = series_of(this);

	if (this->state == WFT_STATE_SYS_ERROR && this->error < 0)
	{
		this->state = WFT_STATE_SSL_ERROR;
		this->error = -this->error;
	}

	if (this->callback)
		this->callback(this);

	delete this;
	return series->pop();
}
```

此处callback依然可以向series里添加任务。这个时候我们server task被delete掉，但是series中还有task，会继续pop出来dispatch执行，显然server task生命周期和series不一样。

## 结论

client和server两种情况，虽然说产生的方式不同，但是到最后执行都是到WFNetworkTask::done，他们结束的方式和时机是相同的，在这里delete this，和series是无关的

update: 在新版代码中，delete操作defer了，详细可见

[task defer analysis](other_04_task_defer_delete.md)

总的来说，任何task生命周期都是到callback结束，callback之后就不能再访问了。而内部实际delete task的时机，不一定是callback之后立刻进行(见新代码解析)。但这个和用户的使用无关，callback之后不能再使用task。