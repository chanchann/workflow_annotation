# workflow 源码解析 13 : http server

项目源码 : https://github.com/sogou/workflow

更加详细的源码注释可看 : https://github.com/chanchann/workflow_annotation

我们先看最简单的http例子

https://github.com/chanchann/workflow_annotation/blob/main/demos/07_http/http_no_reply.cc

## server的交互流程

从上一章我们可以看到，我们accept建立连接后，把read操作挂上了epoll监听

当事件来临，我们epoll_wait拿到这个事件

```cpp
case PD_OP_READ:
    __poller_handle_read(node, poller);
    break;
```

```cpp
static void __poller_handle_read(struct __poller_node *node,
								 poller_t *poller)
{
	while (1)
	{
        // char buf[POLLER_BUFSIZE];
		p = poller->buf;
        ...
        nleft = read(node->data.fd, p, POLLER_BUFSIZE);
        if (nleft < 0)
        {
            if (errno == EAGAIN)
                return;
        }

		if (nleft <= 0)
			break;

		do
		{
			n = nleft;
			if (__poller_append_message(p, &n, node, poller) >= 0)
			{
				nleft -= n;
				p += n;
			}
			else
				nleft = -1;
		} while (nleft > 0);

		if (nleft < 0)
			break;
	}

	if (__poller_remove_node(node, poller))
		return;

	if (nleft == 0)
	{
		node->error = 0;
		node->state = PR_ST_FINISHED;
	}
	else
	{
		node->error = errno;
		node->state = PR_ST_ERROR;
	}

	free(node->res);
    // 结果写道msgqueue
	poller->cb((struct poller_result *)node, poller->ctx);
}
```

主要是read数据到poller->buf中，然后`__poller_append_message`, 最后把结果写到msgqueue中

### __poller_append_message

```cpp

static int __poller_append_message(const void *buf, size_t *n,
								   struct __poller_node *node,
								   poller_t *poller)
{
	poller_message_t *msg = node->data.message;
	struct __poller_node *res;

	if (!msg)
	{
		res = (struct __poller_node *)malloc(sizeof (struct __poller_node));
        ...
		msg = poller->create_message(node->data.context);
        ...
		node->data.message = msg;
		node->res = res;
	}
	else
		res = node->res;

    msg->append(buf, n, msg);

    res->data = node->data;
    res->error = 0;
    res->state = PR_ST_SUCCESS;
    poller->cb((struct poller_result *)res, poller->ctx);

    node->data.message = NULL;
    node->res = NULL;
}
```

其中`node->data.message` 是 `poller_message_t *message;`

把在poller中读到的数据放进msgqueue中处理

## Communicator::handle_read_result

于是消费者msgqueue get来消费啦

```cpp
void Communicator::handle_read_result(struct poller_result *res)
{
	struct CommConnEntry *entry = (struct CommConnEntry *)res->data.context;

	if (res->state != PR_ST_MODIFIED)
	{
		if (entry->service)
			this->handle_incoming_request(res);
		else
			this->handle_incoming_reply(res);
	}
}
```

通过service来判断是否是服务端

如果是服务端肯定是处理request，客户端肯定是req发送后，接收回来的reply

我们这里是server端

## handle_incoming_request

```cpp
void Communicator::handle_incoming_request(struct poller_result *res)
{
	struct CommConnEntry *entry = (struct CommConnEntry *)res->data.context;
	CommTarget *target = entry->target;
	CommSession *session = NULL;

	switch (res->state)
	{
	case PR_ST_SUCCESS:
		session = entry->session;
		state = CS_STATE_TOREPLY;
		pthread_mutex_lock(&target->mutex);
		if (entry->state == CONN_STATE_SUCCESS)
		{
			__sync_add_and_fetch(&entry->ref, 1);
			entry->state = CONN_STATE_IDLE;
			list_add(&entry->list, &target->idle_list);
		}

		pthread_mutex_unlock(&target->mutex);
		break;
        ....

    }
    session->handle(state, res->error);

    if (__sync_sub_and_fetch(&entry->ref, 1) == 0)
    {
        this->release_conn(entry);
        ((CommServiceTarget *)target)->decref();
    }

}

```