/*
  Copyright (c) 2019 Sogou, Inc.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  Author: Xie Han (xiehan@sogou-inc.com)
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#ifdef __linux__
# include <sys/epoll.h>
# include <sys/timerfd.h>
#else
# include <sys/event.h>
# undef LIST_HEAD
# undef SLIST_HEAD
#endif
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "list.h"
#include "rbtree.h"
#include "poller.h"

#define POLLER_BUFSIZE			(256 * 1024)
#define POLLER_NODES_MAX		65536
#define POLLER_EVENTS_MAX		256
#define POLLER_NODE_ERROR		((struct __poller_node *)-1)


/**
 * @brief 为了性能考虑我们的poller_node是一个以fd为下标的数组，而每个node只能关注一种事件，READ或WRITE。
 * todo: 这个数组在哪体现？？
 * 对于一个fd，在poller里是单工的。如果需要全双工，方法是通过系统调用dup()产生一个新的fd再加进来。
 * 
 */
struct __poller_node
{
	int state;    
	int error;
	struct poller_data data;
#pragma pack(1)
	union
	{
		struct list_head list;
		struct rb_node rb;
	};
#pragma pack()
	char in_rbtree;
	char removed;
	int event;
	struct timespec timeout;
	struct __poller_node *res;
};

struct __poller
{
	size_t max_open_files;  // 如果为0则默认为65536
	// 发生读事件时创建一条消息。poller既不是proactor也不是reactor，而是以完整的一条消息为交互单位。
	// 其中的void *参数来自于struct poller_data里的void *context（注意不是poller_params里的context）。
	// poller_message_t是一条完整的消息，这是一个变长结构体，需要而且只需要实现append
	poller_message_t *(*create_message)(void *); 
	// 写的时候表示写成功了一部分数据，一般用来更新下一个超时。参数void *是poller_data里的context。
	int (*partial_written)(size_t, void *);
	// 用于返回结果，参数是一个poller_result和context。
	void (*cb)(struct poller_result *, void *);
	// callback里的void *参数。
	void *ctx;  

	pthread_t tid;
	int pfd;
	int timerfd;
	int pipe_rd;      // pipe 的读端
	int pipe_wr;      // pipe 的写端
	int stopped;     // 是否运行标志位
	struct rb_root timeo_tree;
	struct rb_node *tree_first;
	struct list_head timeo_list;
	struct list_head no_timeo_list;
	struct __poller_node **nodes;
	pthread_mutex_t mutex;
	char buf[POLLER_BUFSIZE];
};

#ifdef __linux__

// 之所以要封装，是因为不同平台，保持同样的接口
static inline int __poller_create_pfd()
{
	return epoll_create(1);
}

static inline int __poller_add_fd(int fd, int event, void *data,
								  poller_t *poller)
{
	struct epoll_event ev = {
		.events		=	event,
		.data		=	{
			.ptr	=	data
		}
	};
	return epoll_ctl(poller->pfd, EPOLL_CTL_ADD, fd, &ev);
}

static inline int __poller_del_fd(int fd, int event, poller_t *poller)
{
	return epoll_ctl(poller->pfd, EPOLL_CTL_DEL, fd, NULL);
}

static inline int __poller_mod_fd(int fd, int old_event,
								  int new_event, void *data,
								  poller_t *poller)
{
	struct epoll_event ev = {
		.events		=	new_event,
		.data		=	{
			.ptr	=	data
		}
	};
	return epoll_ctl(poller->pfd, EPOLL_CTL_MOD, fd, &ev);
}

static inline int __poller_create_timerfd()
{
	return timerfd_create(CLOCK_MONOTONIC, 0);
}

static inline int __poller_add_timerfd(int fd, poller_t *poller)
{
	struct epoll_event ev = {
		.events		=	EPOLLIN | EPOLLET,  // TODO : 为何为ET?
		.data		=	{
			.ptr	=	NULL
		}
	};
	// epoll 上添加上timerfd
	return epoll_ctl(poller->pfd, EPOLL_CTL_ADD, fd, &ev);
}

static inline int __poller_set_timerfd(int fd, const struct timespec *abstime,
									   poller_t *poller)
{
	struct itimerspec timer = {
		.it_interval	=	{ },
		.it_value		=	*abstime
	};
	return timerfd_settime(fd, TFD_TIMER_ABSTIME, &timer, NULL);
}

typedef struct epoll_event __poller_event_t;

static inline int __poller_wait(__poller_event_t *events, int maxevents,
								poller_t *poller)
{
	// int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);
	// Specifying a timeout of -1 causes epoll_wait() to block indefinitely
	return epoll_wait(poller->pfd, events, maxevents, -1);
}

static inline void *__poller_event_data(const __poller_event_t *event)
{
	return event->data.ptr;
}

#else /* BSD, macOS */

static inline int __poller_create_pfd()
{
	return kqueue();
}

static inline int __poller_add_fd(int fd, int event, void *data,
								  poller_t *poller)
{
	struct kevent ev;
	EV_SET(&ev, fd, event, EV_ADD, 0, 0, data);
	return kevent(poller->pfd, &ev, 1, NULL, 0, NULL);
}

static inline int __poller_del_fd(int fd, int event, poller_t *poller)
{
	struct kevent ev;
	EV_SET(&ev, fd, event, EV_DELETE, 0, 0, NULL);
	return kevent(poller->pfd, &ev, 1, NULL, 0, NULL);
}

static inline int __poller_mod_fd(int fd, int old_event,
								  int new_event, void *data,
								  poller_t *poller)
{
	struct kevent ev[2];
	EV_SET(&ev[0], fd, old_event, EV_DELETE, 0, 0, NULL);
	EV_SET(&ev[1], fd, new_event, EV_ADD, 0, 0, data);
	return kevent(poller->pfd, ev, 2, NULL, 0, NULL);
}

static inline int __poller_create_timerfd()
{
	return dup(0);
}

static inline int __poller_add_timerfd(int fd, poller_t *poller)
{
	return 0;
}

static int __poller_set_timerfd(int fd, const struct timespec *abstime,
								poller_t *poller)
{
	struct timespec curtime;
	long long nseconds;
	struct kevent ev;
	int flags;

	if (abstime->tv_sec || abstime->tv_nsec)
	{
		clock_gettime(CLOCK_MONOTONIC, &curtime);
		nseconds = 1000000000LL * (abstime->tv_sec - curtime.tv_sec);
		nseconds += abstime->tv_nsec - curtime.tv_nsec;
		flags = EV_ADD;
	}
	else
	{
		nseconds = 0;
		flags = EV_DELETE;
	}

	EV_SET(&ev, fd, EVFILT_TIMER, flags, NOTE_NSECONDS, nseconds, NULL);
	return kevent(poller->pfd, &ev, 1, NULL, 0, NULL);
}

typedef struct kevent __poller_event_t;

static inline int __poller_wait(__poller_event_t *events, int maxevents,
								poller_t *poller)
{
	return kevent(poller->pfd, NULL, 0, events, maxevents, NULL);
}

static inline void *__poller_event_data(const __poller_event_t *event)
{
	return event->udata;
}

#define EPOLLIN		EVFILT_READ
#define EPOLLOUT	EVFILT_WRITE
#define EPOLLET		0

#endif

static inline long __timeout_cmp(const struct __poller_node *node1,
								 const struct __poller_node *node2)
{
	long ret = node1->timeout.tv_sec - node2->timeout.tv_sec;

	if (ret == 0)
		ret = node1->timeout.tv_nsec - node2->timeout.tv_nsec;

	return ret;
}

static void __poller_tree_insert(struct __poller_node *node, poller_t *poller)
{
	struct rb_node **p = &poller->timeo_tree.rb_node;
	struct rb_node *parent = NULL;
	struct __poller_node *entry;
	int first = 1;

	while (*p)
	{
		parent = *p;
		entry = rb_entry(*p, struct __poller_node, rb);
		if (__timeout_cmp(node, entry) < 0)
			p = &(*p)->rb_left;
		else
		{
			p = &(*p)->rb_right;
			first = 0;
		}
	}

	if (first)
		poller->tree_first = &node->rb;

	node->in_rbtree = 1;
	rb_link_node(&node->rb, parent, p);
	rb_insert_color(&node->rb, &poller->timeo_tree);
}

static inline void __poller_tree_erase(struct __poller_node *node,
									   poller_t *poller)
{
	if (&node->rb == poller->tree_first)
		poller->tree_first = rb_next(&node->rb);

	rb_erase(&node->rb, &poller->timeo_tree);
	node->in_rbtree = 0;
}

static int __poller_remove_node(struct __poller_node *node, poller_t *poller)
{
	int removed;

	pthread_mutex_lock(&poller->mutex);
	removed = node->removed;
	if (!removed)
	{
		poller->nodes[node->data.fd] = NULL;

		if (node->in_rbtree)
			__poller_tree_erase(node, poller);
		else
			list_del(&node->list);

		__poller_del_fd(node->data.fd, node->event, poller);
	}

	pthread_mutex_unlock(&poller->mutex);
	return removed;
}

static int __poller_append_message(const void *buf, size_t *n,
								   struct __poller_node *node,
								   poller_t *poller)
{
	poller_message_t *msg = node->data.message;
	struct __poller_node *res;
	int ret;

	if (!msg)
	{
		res = (struct __poller_node *)malloc(sizeof (struct __poller_node));
		if (!res)
			return -1;

		msg = poller->create_message(node->data.context);
		if (!msg)
		{
			free(res);
			return -1;
		}

		node->data.message = msg;
		node->res = res;
	}
	else
		res = node->res;

	ret = msg->append(buf, n, msg);
	if (ret > 0)
	{
		res->data = node->data;
		res->error = 0;
		res->state = PR_ST_SUCCESS;
		poller->cb((struct poller_result *)res, poller->ctx);

		node->data.message = NULL;
		node->res = NULL;
	}

	return ret;
}

static int __poller_handle_ssl_error(struct __poller_node *node, int ret,
									 poller_t *poller)
{
	int error = SSL_get_error(node->data.ssl, ret);
	int event;

	switch (error)
	{
	case SSL_ERROR_WANT_READ:
		event = EPOLLIN | EPOLLET;
		break;
	case SSL_ERROR_WANT_WRITE:
		event = EPOLLOUT | EPOLLET;
		break;
	default:
		errno = -error;
	case SSL_ERROR_SYSCALL:
		return -1;
	}

	if (event == node->event)
		return 0;

	pthread_mutex_lock(&poller->mutex);
	if (!node->removed)
	{
		ret = __poller_mod_fd(node->data.fd, node->event, event, node, poller);
		if (ret >= 0)
			node->event = event;
	}
	else
		ret = 0;

	pthread_mutex_unlock(&poller->mutex);
	return ret;
}

static void __poller_handle_read(struct __poller_node *node,
								 poller_t *poller)
{
	ssize_t nleft;
	size_t n;
	char *p;

	while (1)
	{
		p = poller->buf;
		if (node->data.ssl)
		{
			nleft = SSL_read(node->data.ssl, p, POLLER_BUFSIZE);
			if (nleft < 0)
			{
				if (__poller_handle_ssl_error(node, nleft, poller) >= 0)
					return;
			}
		}
		else
		{
			nleft = read(node->data.fd, p, POLLER_BUFSIZE);
			if (nleft < 0)
			{
				if (errno == EAGAIN)
					return;
			}
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
	poller->cb((struct poller_result *)node, poller->ctx);
}

#ifndef IOV_MAX
# ifdef UIO_MAXIOV
#  define IOV_MAX	UIO_MAXIOV
# else
#  define IOV_MAX	1024
# endif
#endif

static void __poller_handle_write(struct __poller_node *node,
								  poller_t *poller)
{
	struct iovec *iov = node->data.write_iov;
	size_t count = 0;
	ssize_t nleft;
	int iovcnt;
	int ret;

	while (node->data.iovcnt > 0 && iov->iov_len == 0)
	{
		iov++;
		node->data.iovcnt--;
	}

	while (node->data.iovcnt > 0)
	{
		if (node->data.ssl)
		{
			nleft = SSL_write(node->data.ssl, iov->iov_base, iov->iov_len);
			if (nleft <= 0)
			{
				ret = __poller_handle_ssl_error(node, nleft, poller);
				break;
			}
		}
		else
		{
			iovcnt = node->data.iovcnt;
			if (iovcnt > IOV_MAX)
				iovcnt = IOV_MAX;

			nleft = writev(node->data.fd, iov, iovcnt);
			if (nleft < 0)
			{
				ret = errno == EAGAIN ? 0 : -1;
				break;
			}
		}

		count += nleft;
		do
		{
			if (nleft >= iov->iov_len)
			{
				nleft -= iov->iov_len;
				iov->iov_base = (char *)iov->iov_base + iov->iov_len;
				iov->iov_len = 0;
				iov++;
				node->data.iovcnt--;
			}
			else
			{
				iov->iov_base = (char *)iov->iov_base + nleft;
				iov->iov_len -= nleft;
				break;
			}
		} while (node->data.iovcnt > 0);
	}

	node->data.write_iov = iov;
	if (node->data.iovcnt > 0 && ret >= 0)
	{
		if (count == 0)
			return;

		if (poller->partial_written(count, node->data.context) >= 0)
			return;
	}

	if (__poller_remove_node(node, poller))
		return;

	if (node->data.iovcnt == 0)
	{
		node->error = 0;
		node->state = PR_ST_FINISHED;
	}
	else
	{
		node->error = errno;
		node->state = PR_ST_ERROR;
	}

	poller->cb((struct poller_result *)node, poller->ctx);
}

static void __poller_handle_listen(struct __poller_node *node,
								   poller_t *poller)
{
	struct __poller_node *res = node->res;
	struct sockaddr_storage ss;
	socklen_t len;
	int sockfd;
	void *p;

	while (1)
	{
		len = sizeof (struct sockaddr_storage);
		sockfd = accept(node->data.fd, (struct sockaddr *)&ss, &len);
		if (sockfd < 0)
		{
			if (errno == EAGAIN)
				return;
			else
				break;
		}

		p = node->data.accept((const struct sockaddr *)&ss, len,
							  sockfd, node->data.context);
		if (!p)
			break;

		res->data = node->data;
		res->data.result = p;
		res->error = 0;
		res->state = PR_ST_SUCCESS;
		poller->cb((struct poller_result *)res, poller->ctx);

		res = (struct __poller_node *)malloc(sizeof (struct __poller_node));
		node->res = res;
		if (!res)
			break;
	}

	if (__poller_remove_node(node, poller))
		return;

	node->error = errno;
	node->state = PR_ST_ERROR;
	free(node->res);
	poller->cb((struct poller_result *)node, poller->ctx);
}

static void __poller_handle_connect(struct __poller_node *node,
									poller_t *poller)
{
	socklen_t len = sizeof (int);
	int error;

	if (getsockopt(node->data.fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0)
		error = errno;

	if (__poller_remove_node(node, poller))
		return;

	if (error == 0)
	{
		node->error = 0;
		node->state = PR_ST_FINISHED;
	}
	else
	{
		node->error = error;
		node->state = PR_ST_ERROR;
	}

	poller->cb((struct poller_result *)node, poller->ctx);
}

static void __poller_handle_ssl_accept(struct __poller_node *node,
									   poller_t *poller)
{
	int ret = SSL_accept(node->data.ssl);

	if (ret <= 0)
	{
		if (__poller_handle_ssl_error(node, ret, poller) >= 0)
			return;
	}

	if (__poller_remove_node(node, poller))
		return;

	if (ret > 0)
	{
		node->error = 0;
		node->state = PR_ST_FINISHED;
	}
	else
	{
		node->error = errno;
		node->state = PR_ST_ERROR;
	}

	poller->cb((struct poller_result *)node, poller->ctx);
}

static void __poller_handle_ssl_connect(struct __poller_node *node,
										poller_t *poller)
{
	int ret = SSL_connect(node->data.ssl);

	if (ret <= 0)
	{
		if (__poller_handle_ssl_error(node, ret, poller) >= 0)
			return;
	}

	if (__poller_remove_node(node, poller))
		return;

	if (ret > 0)
	{
		node->error = 0;
		node->state = PR_ST_FINISHED;
	}
	else
	{
		node->error = errno;
		node->state = PR_ST_ERROR;
	}

	poller->cb((struct poller_result *)node, poller->ctx);
}

static void __poller_handle_ssl_shutdown(struct __poller_node *node,
										 poller_t *poller)
{
	int ret = SSL_shutdown(node->data.ssl);

	if (ret <= 0)
	{
		if (__poller_handle_ssl_error(node, ret, poller) >= 0)
			return;
	}

	if (__poller_remove_node(node, poller))
		return;

	if (ret > 0)
	{
		node->error = 0;
		node->state = PR_ST_FINISHED;
	}
	else
	{
		node->error = errno;
		node->state = PR_ST_ERROR;
	}

	poller->cb((struct poller_result *)node, poller->ctx);
}

static void __poller_handle_event(struct __poller_node *node,
								  poller_t *poller)
{
	struct __poller_node *res = node->res;
	unsigned long long cnt = 0;
	unsigned long long value;
	ssize_t ret;
	void *p;

	while (1)
	{
		ret = read(node->data.fd, &value, sizeof (unsigned long long));
		if (ret == sizeof (unsigned long long))
			cnt += value;
		else
		{
			if (ret >= 0)
				errno = EINVAL;
			break;
		}
	}

	if (errno == EAGAIN)
	{
		while (1)
		{
			if (cnt == 0)
				return;

			cnt--;
			p = node->data.event(node->data.context);
			if (!p)
				break;

			res->data = node->data;
			res->data.result = p;
			res->error = 0;
			res->state = PR_ST_SUCCESS;
			poller->cb((struct poller_result *)res, poller->ctx);

			res = (struct __poller_node *)malloc(sizeof (struct __poller_node));
			node->res = res;
			if (!res)
				break;
		}
	}

	if (cnt != 0)
		write(node->data.fd, &cnt, sizeof (unsigned long long));

	if (__poller_remove_node(node, poller))
		return;

	node->error = errno;
	node->state = PR_ST_ERROR;
	free(node->res);
	poller->cb((struct poller_result *)node, poller->ctx);
}

static void __poller_handle_notify(struct __poller_node *node,
								   poller_t *poller)
{
	struct __poller_node *res = node->res;
	ssize_t ret;
	void *p;

	while (1)
	{
		ret = read(node->data.fd, &p, sizeof (void *));
		if (ret == sizeof (void *))
		{
			p = node->data.notify(p, node->data.context);
			if (!p)
				break;

			res->data = node->data;
			res->data.result = p;
			res->error = 0;
			res->state = PR_ST_SUCCESS;
			poller->cb((struct poller_result *)res, poller->ctx);

			res = (struct __poller_node *)malloc(sizeof (struct __poller_node));
			node->res = res;
			if (!res)
				break;
		}
		else if (ret < 0 && errno == EAGAIN)
			return;
		else
		{
			if (ret > 0)
				errno = EINVAL;
			break;
		}
	}

	if (__poller_remove_node(node, poller))
		return;

	if (ret == 0)
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
	poller->cb((struct poller_result *)node, poller->ctx);
}

static int __poller_handle_pipe(poller_t *poller)
{
	struct __poller_node **node = (struct __poller_node **)poller->buf;
	int stop = 0;
	int n;
	int i;

	n = read(poller->pipe_rd, node, POLLER_BUFSIZE) / sizeof (void *);
	for (i = 0; i < n; i++)
	{
		if (node[i])
		{
			free(node[i]->res);
			poller->cb((struct poller_result *)node[i], poller->ctx);
		}
		else
			stop = 1;
	}

	return stop;
}

static void __poller_handle_timeout(const struct __poller_node *time_node,
									poller_t *poller)
{
	struct __poller_node *node;
	struct list_head *pos, *tmp;
	LIST_HEAD(timeo_list);

	pthread_mutex_lock(&poller->mutex);
	list_for_each_safe(pos, tmp, &poller->timeo_list)
	{
		node = list_entry(pos, struct __poller_node, list);
		if (__timeout_cmp(node, time_node) <= 0)
		{
			if (node->data.fd >= 0)
			{
				poller->nodes[node->data.fd] = NULL;
				__poller_del_fd(node->data.fd, node->event, poller);
			}

			list_move_tail(pos, &timeo_list);
		}
		else
			break;
	}

	while (poller->tree_first)
	{
		node = rb_entry(poller->tree_first, struct __poller_node, rb);
		if (__timeout_cmp(node, time_node) < 0)
		{
			if (node->data.fd >= 0)
			{
				poller->nodes[node->data.fd] = NULL;
				__poller_del_fd(node->data.fd, node->event, poller);
			}

			poller->tree_first = rb_next(poller->tree_first);
			rb_erase(&node->rb, &poller->timeo_tree);
			list_add_tail(&node->list, &timeo_list);
		}
		else
			break;
	}

	pthread_mutex_unlock(&poller->mutex);
	while (!list_empty(&timeo_list))
	{
		node = list_entry(timeo_list.next, struct __poller_node, list);
		list_del(&node->list);

		node->error = ETIMEDOUT;
		node->state = PR_ST_ERROR;
		free(node->res);
		poller->cb((struct poller_result *)node, poller->ctx);
	}
}

static void __poller_set_timer(poller_t *poller)
{
	struct __poller_node *node = NULL;
	struct __poller_node *first;
	struct timespec abstime;

	pthread_mutex_lock(&poller->mutex);
	if (!list_empty(&poller->timeo_list))
		node = list_entry(poller->timeo_list.next, struct __poller_node, list);

	if (poller->tree_first)
	{
		first = rb_entry(poller->tree_first, struct __poller_node, rb);
		if (!node || __timeout_cmp(first, node) < 0)
			node = first;
	}

	if (node)
		abstime = node->timeout;
	else
	{
		abstime.tv_sec = 0;
		abstime.tv_nsec = 0;
	}

	__poller_set_timerfd(poller->timerfd, &abstime, poller);
	pthread_mutex_unlock(&poller->mutex);
}

// 检测事件核心部分
static void *__poller_thread_routine(void *arg)
{
	// 这里传入的参数就是poller
	/*
		struct __poller
		{
			size_t max_open_files;
			poller_message_t *(*create_message)(void *);
			int (*partial_written)(size_t, void *);
			void (*cb)(struct poller_result *, void *);
			void *ctx;

			pthread_t tid;
			int pfd;
			int timerfd;
			int pipe_rd;
			int pipe_wr;
			int stopped;
			struct rb_root timeo_tree;
			struct rb_node *tree_first;
			struct list_head timeo_list;
			struct list_head no_timeo_list;
			struct __poller_node **nodes;
			pthread_mutex_t mutex;
			char buf[POLLER_BUFSIZE];
		};
	*/
	poller_t *poller = (poller_t *)arg;

	// typedef struct epoll_event __poller_event_t;
	// 这里就是一个epoll_event 数组
	__poller_event_t events[POLLER_EVENTS_MAX];
	/*
		struct __poller_node
		{
			int state;    
			int error;
			struct poller_data data;
		#pragma pack(1)
			union
			{
				struct list_head list;
				struct rb_node rb;
			};
		#pragma pack()
			char in_rbtree;
			char removed;
			int event;
			struct timespec timeout;
			struct __poller_node *res;
		};
	*/
	struct __poller_node time_node;
	struct __poller_node *node;
	int has_pipe_event;
	int nevents;
	int i;

	while (1)
	{
		__poller_set_timer(poller);
		//  __poller_wait 调用在linux下是通过 epoll_wait 来检测 fd 发生的事件。
		// nevents 是返回的事件，fd及其事件信息被保存在 events 这个 epoll_event 结构体数组中
		nevents = __poller_wait(events, POLLER_EVENTS_MAX, poller);
		clock_gettime(CLOCK_MONOTONIC, &time_node.timeout);
		has_pipe_event = 0;
		for (i = 0; i < nevents; i++)
		{
			node = (struct __poller_node *)__poller_event_data(&events[i]);
			// (struct __poller_node *)1 的含义 :
			// 在event的data里，用0和1分别代表定时器和管道事件。
			// 因为data是一个指针，所以直接就把数值1强转成指针
			// 1不可能是一个合法地址，所以看到node==1就知道是一个pipe事件
			// pipe用来通知各种fd的删除和poller的停止
			// 这里判断if (node>1)，是因为大多数情况下，都是正常的网络事件，于是只需判断一次，就能进入处理逻辑了
			if (node > (struct __poller_node *)1)  
			{
				// 一般我们用epoll_event.events 是操作系统告诉本程序该fd当前发生的事件类型。
				// 但是下面判断 fd 发生的事件类型是通过 node->data.operation 来判断的。
				// 为了性能考虑我们的poller_node是一个以fd为下标的数组，
				// 而每个node只能关注一种事件，READ或WRITE。
				// 所有我们需要通过operation来判断调用哪个处理函数，而不能通过event来判断

				// 为了性能考虑我们的poller_node是一个以fd为下标的数组，而每个node只能关注一种事件，READ或WRITE。
				// 所有我们需要通过operation来判断调用哪个处理函数，而不能通过event来判断。
				switch (node->data.operation)
				{
				case PD_OP_READ:
					__poller_handle_read(node, poller);
					break;
				case PD_OP_WRITE:
					// 测试中发现，向对等方发送消息均是通过Communicator::send_message_sync中的writev(entry->sockfd, vectors, cnt <= IOV_MAX ? cnt : IOV_MAX)来实现，
					// 那此处中的__poller_handle_write的功能是？
					// A : 消息一次发得出去，就不走异步写了。这样子快。试个大一点的消息，就会进poller了。
					__poller_handle_write(node, poller);
					break;
				case PD_OP_LISTEN:
					__poller_handle_listen(node, poller);
					break;
				case PD_OP_CONNECT:
					__poller_handle_connect(node, poller);
					break;
				case PD_OP_SSL_ACCEPT:
					__poller_handle_ssl_accept(node, poller);
					break;
				case PD_OP_SSL_CONNECT:
					__poller_handle_ssl_connect(node, poller);
					break;
				case PD_OP_SSL_SHUTDOWN:
					__poller_handle_ssl_shutdown(node, poller);
					break;
				case PD_OP_EVENT:
					// event其实是linux的eventfd。
					__poller_handle_event(node, poller);
					break;
				case PD_OP_NOTIFY:
					// notify忽略，只有mac下的异步文件IO才使用到的。
					__poller_handle_notify(node, poller);
					break;
				}
			}
			else if (node == (struct __poller_node *)1)  // 1 为 pipe事件
				has_pipe_event = 1;
		}

		if (has_pipe_event)
		{
			if (__poller_handle_pipe(poller))
				break;
		}

		__poller_handle_timeout(&time_node, poller);
	}

#if OPENSSL_VERSION_NUMBER < 0x10100000L
# ifdef CRYPTO_LOCK_ECDH
	ERR_remove_thread_state(NULL);
# else
	ERR_remove_state(0);
# endif
#endif
	return NULL;
}

static int __poller_open_pipe(poller_t *poller)
{
	int pipefd[2];

	if (pipe(pipefd) >= 0)
	{
		if (__poller_add_fd(pipefd[0], EPOLLIN, (void *)1, poller) >= 0)
		{
			poller->pipe_rd = pipefd[0];
			poller->pipe_wr = pipefd[1];
			return 0;
		}

		close(pipefd[0]);
		close(pipefd[1]);
	}

	return -1;
}

/**
 * @brief 给poller添加timerfd
 * 
 * @param poller 
 * @return int 0 成功， -1 失败
 */
static int __poller_create_timer(poller_t *poller)
{
	int timerfd = __poller_create_timerfd();  // !!! 到底了， timerfd_create

	if (timerfd >= 0) 
	{
		if (__poller_add_timerfd(timerfd, poller) >= 0)
		{
			poller->timerfd = timerfd;
			return 0;
		}
		// __poller_add_timerfd失败
		close(timerfd);
	}

	return -1;
}

/**
 * @brief 产生一个poller对象。需要传一个params参数，包含的域在poller_t中
 * 
 * 
 * @param params 
 * @return poller_t* 
 */
poller_t *poller_create(const struct poller_params *params)
{
	poller_t *poller = (poller_t *)malloc(sizeof (poller_t));
	size_t n;
	int ret;

	if (!poller)  // malloc 失败
		return NULL;

	n = params->max_open_files;
	if (n == 0)
		n = POLLER_NODES_MAX;  // 如果取的params默认为0， 则设置为max nodes(65536)

    // calloc() gives you a zero-initialized buffer, while malloc() leaves the memory uninitialized.
    // https://stackoverflow.com/questions/1538420/difference-between-malloc-and-calloc
	poller->nodes = (struct __poller_node **)calloc(n, sizeof (void *));  // 分配max_open_files 这么多的nodes
	if (poller->nodes)
	{
		poller->pfd = __poller_create_pfd();   // !!!这里终于到底了了，去调用了epoll_create(1);
		if (poller->pfd >= 0)  
		{
			if (__poller_create_timer(poller) >= 0)   // 给epoll添加timerfd
			{
				// The pthread_mutex_init() function shall initialize the mutex referenced by mutex with attributes specified by attr.
				// https://linux.die.net/man/3/pthread_mutex_init
				ret = pthread_mutex_init(&poller->mutex, NULL);
				if (ret == 0)  // pthread_mutex_init successfully
				{
					// 通过params设置poller的各个参数
					poller->max_open_files = n;
					poller->create_message = params->create_message;
					poller->partial_written = params->partial_written;
					poller->cb = params->callback;
					poller->ctx = params->context;

					poller->timeo_tree.rb_node = NULL;
					poller->tree_first = NULL;
					INIT_LIST_HEAD(&poller->timeo_list);
					INIT_LIST_HEAD(&poller->no_timeo_list);
					poller->nodes[poller->timerfd] = POLLER_NODE_ERROR;
					poller->nodes[poller->pfd] = POLLER_NODE_ERROR;
					poller->stopped = 1;  
					return poller;
				}

				errno = ret;
				close(poller->timerfd);  // __poller_create_timer 失败
			}

			close(poller->pfd);
		}
		// epoll_create 失败
		free(poller->nodes);
	}
	free(poller); // if poller->nodes calloc 失败
	return NULL;
}

/**
 * @brief 销毁停止状态的poller。运行中的poller直接destroy的话，行为无定义。
 * 
 * @param poller 
 */
void poller_destroy(poller_t *poller)
{
	pthread_mutex_destroy(&poller->mutex);
	close(poller->timerfd);
	close(poller->pfd);
	free(poller->nodes);
	free(poller);
}

/**
 * @brief 主要作用就是开启__poller_thread_routine线程
 * 启动poller。poller被创建后，是处于停止状态。调用poller_start可以让poller开始运行。
 * 
 * @param poller 
 * @return int 
 */
int poller_start(poller_t *poller)
{
	pthread_t tid;
	int ret;

	pthread_mutex_lock(&poller->mutex);
	if (__poller_open_pipe(poller) >= 0)
	{
		ret = pthread_create(&tid, NULL, __poller_thread_routine, poller);
		if (ret == 0)
		{
			poller->tid = tid;
			poller->nodes[poller->pipe_rd] = POLLER_NODE_ERROR;   // todo : 设置这个干嘛
			poller->nodes[poller->pipe_wr] = POLLER_NODE_ERROR;
			poller->stopped = 0;
		}
		else
		{
			errno = ret;
			close(poller->pipe_wr);
			close(poller->pipe_rd);
		}
	}

	pthread_mutex_unlock(&poller->mutex);
	return -poller->stopped;
}

static void __poller_insert_node(struct __poller_node *node,
								 poller_t *poller)
{
	struct __poller_node *end;

	end = list_entry(poller->timeo_list.prev, struct __poller_node, list);
	if (list_empty(&poller->timeo_list) || __timeout_cmp(node, end) >= 0)
		list_add_tail(&node->list, &poller->timeo_list);
	else
		__poller_tree_insert(node, poller);

	if (&node->list == poller->timeo_list.next)
	{
		if (poller->tree_first)
			end = rb_entry(poller->tree_first, struct __poller_node, rb);
		else
			end = NULL;
	}
	else if (&node->rb == poller->tree_first)
		end = list_entry(poller->timeo_list.next, struct __poller_node, list);
	else
		return;

	if (!end || __timeout_cmp(node, end) < 0)
		__poller_set_timerfd(poller->timerfd, &node->timeout, poller);
}

static void __poller_node_set_timeout(int timeout, struct __poller_node *node)
{
	clock_gettime(CLOCK_MONOTONIC, &node->timeout);
	node->timeout.tv_sec += timeout / 1000;
	node->timeout.tv_nsec += timeout % 1000 * 1000000;
	if (node->timeout.tv_nsec >= 1000000000)
	{
		node->timeout.tv_nsec -= 1000000000;
		node->timeout.tv_sec++;
	}
}

static int __poller_data_get_event(int *event, const struct poller_data *data)
{
	switch (data->operation)
	{
	case PD_OP_READ:
		*event = EPOLLIN | EPOLLET;
		return !!data->message;
	case PD_OP_WRITE:
		*event = EPOLLOUT | EPOLLET;
		return 0;
	case PD_OP_LISTEN:
		*event = EPOLLIN | EPOLLET;
		return 1;
	case PD_OP_CONNECT:
		*event = EPOLLOUT | EPOLLET;
		return 0;
	case PD_OP_SSL_ACCEPT:
		*event = EPOLLIN | EPOLLET;
		return 0;
	case PD_OP_SSL_CONNECT:
		*event = EPOLLOUT | EPOLLET;
		return 0;
	case PD_OP_SSL_SHUTDOWN:
		*event = EPOLLOUT | EPOLLET;
		return 0;
	case PD_OP_EVENT:
		*event = EPOLLIN | EPOLLET;
		return 1;
	case PD_OP_NOTIFY:
		*event = EPOLLIN | EPOLLET;
		return 1;
	default:
		errno = EINVAL;
		return -1;
	}
}

/**
 * @brief poller_add向poller里添加一个fd，fd必须是nonblocking的，否则可能堵死内部线程。
 * fd的操作由struct poller_data的operation决定。
 * 
 * @param data 
 * @param timeout t表示毫秒级的超时（-1表示无限，同epoll风格）。
 * 					达到这个超时时间，fd以ERROR状态从callback返回，错误码（struct poller_result里的error）为ETIMEDOUT。
 * @param poller 
 * @return int 返回0表示添加成功，-1表示失败。
 * 			同一个fd如果添加两次，那么第二次添加返回-1，而且errno==EEXIST。
 * 			因此，如果需要在同一个poller里同时添加一个fd的读与写，需要通过dup()系统调用产生一个复制再添加。
 */
int poller_add(const struct poller_data *data, int timeout, poller_t *poller)
{
	struct __poller_node *res = NULL;
	struct __poller_node *node;
	int need_res;
	int event;

	if ((size_t)data->fd >= poller->max_open_files)
	{
		errno = data->fd < 0 ? EBADF : EMFILE;
		return -1;
	}

	need_res = __poller_data_get_event(&event, data);
	if (need_res < 0)
		return -1;

	if (need_res)
	{
		res = (struct __poller_node *)malloc(sizeof (struct __poller_node));
		if (!res)
			return -1;
	}

	node = (struct __poller_node *)malloc(sizeof (struct __poller_node));
	if (node)
	{
		node->data = *data;
		node->event = event;
		node->in_rbtree = 0;
		node->removed = 0;
		node->res = res;
		if (timeout >= 0)
			__poller_node_set_timeout(timeout, node);

		pthread_mutex_lock(&poller->mutex);
		if (!poller->nodes[data->fd])
		{
			if (__poller_add_fd(data->fd, event, node, poller) >= 0)
			{
				if (timeout >= 0)
					__poller_insert_node(node, poller);
				else
					list_add_tail(&node->list, &poller->no_timeo_list);

				poller->nodes[data->fd] = node;
				node = NULL;
			}
		}
		else if (poller->nodes[data->fd] == POLLER_NODE_ERROR)
			errno = EINVAL;
		else
			errno = EEXIST;

		pthread_mutex_unlock(&poller->mutex);
		if (node == NULL)
			return 0;

		free(node);
	}

	free(res);
	return -1;
}

/**
 * @brief 删除一个fd。
 * 
 * @param fd 
 * @param poller 
 * @return int 返回0表示成果，返回-1表示失败。如果fd不存在，返回-1且errno==ENOENT。
 * 			当poller_del返回0，这个fd必然以DELETED状态从callback里返回。
 */
int poller_del(int fd, poller_t *poller)
{
	struct __poller_node *node;

	if ((size_t)fd >= poller->max_open_files)
	{
		errno = fd < 0 ? EBADF : EMFILE;
		return -1;
	}

	pthread_mutex_lock(&poller->mutex);
	node = poller->nodes[fd];
	if (node)
	{
		poller->nodes[fd] = NULL;

		if (node->in_rbtree)
			__poller_tree_erase(node, poller);
		else
			list_del(&node->list);

		__poller_del_fd(fd, node->event, poller);

		node->error = 0;
		node->state = PR_ST_DELETED;
		if (poller->stopped)
		{
			free(node->res);
			poller->cb((struct poller_result *)node, poller->ctx);
		}
		else
		{
			node->removed = 1;
			write(poller->pipe_wr, &node, sizeof (void *));
		}
	}
	else
		errno = ENOENT;

	pthread_mutex_unlock(&poller->mutex);
	return -!node;
}

int poller_mod(const struct poller_data *data, int timeout, poller_t *poller)
{
	struct __poller_node *res = NULL;
	struct __poller_node *node;
	struct __poller_node *old;
	int need_res;
	int event;

	if ((size_t)data->fd >= poller->max_open_files)
	{
		errno = data->fd < 0 ? EBADF : EMFILE;
		return -1;
	}

	need_res = __poller_data_get_event(&event, data);
	if (need_res < 0)
		return -1;

	if (need_res)
	{
		res = (struct __poller_node *)malloc(sizeof (struct __poller_node));
		if (!res)
			return -1;
	}

	node = (struct __poller_node *)malloc(sizeof (struct __poller_node));
	if (node)
	{
		node->data = *data;
		node->event = event;
		node->in_rbtree = 0;
		node->removed = 0;
		node->res = res;
		if (timeout >= 0)
			__poller_node_set_timeout(timeout, node);

		pthread_mutex_lock(&poller->mutex);
		old = poller->nodes[data->fd];
		if (old && old != POLLER_NODE_ERROR)
		{
			if (__poller_mod_fd(data->fd, old->event, event, node, poller) >= 0)
			{
				if (old->in_rbtree)
					__poller_tree_erase(old, poller);
				else
					list_del(&old->list);

				old->error = 0;
				old->state = PR_ST_MODIFIED;
				if (poller->stopped)
				{
					free(old->res);
					poller->cb((struct poller_result *)old, poller->ctx);
				}
				else
				{
					old->removed = 1;
					write(poller->pipe_wr, &old, sizeof (void *));
				}

				if (timeout >= 0)
					__poller_insert_node(node, poller);
				else
					list_add_tail(&node->list, &poller->no_timeo_list);

				poller->nodes[data->fd] = node;
				node = NULL;
			}
		}
		else if (old == POLLER_NODE_ERROR)
			errno = EINVAL;
		else
			errno = ENOENT;

		pthread_mutex_unlock(&poller->mutex);
		if (node == NULL)
			return 0;

		free(node);
	}

	free(res);
	return -1;
}

/**
 * @brief 重新设置fd的超时
 * 
 * @param fd 
 * @param timeout timeout定义与poller_add一样
 * @param poller 
 * @return int 返回0表示成功，返回-1表示失败。返回-1且errno==ENOENT时，表示fd不存在。
 */
int poller_set_timeout(int fd, int timeout, poller_t *poller)
{
	struct __poller_node time_node;
	struct __poller_node *node;

	if ((size_t)fd >= poller->max_open_files)
	{
		errno = fd < 0 ? EBADF : EMFILE;
		return -1;
	}

	if (timeout >= 0)
		__poller_node_set_timeout(timeout, &time_node);

	pthread_mutex_lock(&poller->mutex);
	node = poller->nodes[fd];
	if (node)
	{
		if (node->in_rbtree)
			__poller_tree_erase(node, poller);
		else
			list_del(&node->list);

		if (timeout >= 0)
		{
			node->timeout = time_node.timeout;
			__poller_insert_node(node, poller);
		}
		else
			list_add_tail(&node->list, &poller->no_timeo_list);
	}
	else
		errno = ENOENT;

	pthread_mutex_unlock(&poller->mutex);
	return -!node;
}

int poller_add_timer(const struct timespec *value, void *context,
					 poller_t *poller)
{
	struct __poller_node *node;

	node = (struct __poller_node *)malloc(sizeof (struct __poller_node));
	if (node)
	{
		memset(&node->data, 0, sizeof (struct poller_data));
		node->data.operation = PD_OP_TIMER;
		node->data.fd = -1;
		node->data.context = context;
		node->in_rbtree = 0;
		node->removed = 0;
		node->res = NULL;

		clock_gettime(CLOCK_MONOTONIC, &node->timeout);
		node->timeout.tv_sec += value->tv_sec;
		node->timeout.tv_nsec += value->tv_nsec;
		if (node->timeout.tv_nsec >= 1000000000)
		{
			node->timeout.tv_nsec -= 1000000000;
			node->timeout.tv_sec++;
		}

		pthread_mutex_lock(&poller->mutex);
		__poller_insert_node(node, poller);
		pthread_mutex_unlock(&poller->mutex);
		return 0;
	}

	return -1;
}

/**
 * @brief 停止poller
 * 当poller被停止时，所有处理中的fd以STOPPED状态从callback里返回。
 * poller停止之后，可以重新start。
 * start与stop显然必须串行依次调用。否则行为无定义。
 * @param poller 
 */
void poller_stop(poller_t *poller)
{
	struct __poller_node *node;
	struct list_head *pos, *tmp;
	void *p = NULL;

	write(poller->pipe_wr, &p, sizeof (void *));
	pthread_join(poller->tid, NULL);
	poller->stopped = 1;

	pthread_mutex_lock(&poller->mutex);
	poller->nodes[poller->pipe_rd] = NULL;
	poller->nodes[poller->pipe_wr] = NULL;
	close(poller->pipe_wr);
	__poller_handle_pipe(poller);
	close(poller->pipe_rd);

	poller->tree_first = NULL;
	while (poller->timeo_tree.rb_node)
	{
		node = rb_entry(poller->timeo_tree.rb_node, struct __poller_node, rb);
		rb_erase(&node->rb, &poller->timeo_tree);
		list_add(&node->list, &poller->timeo_list);
	}

	list_splice_init(&poller->no_timeo_list, &poller->timeo_list);
	list_for_each_safe(pos, tmp, &poller->timeo_list)
	{
		node = list_entry(pos, struct __poller_node, list);
		list_del(&node->list);
		if (node->data.fd >= 0)
		{
			poller->nodes[node->data.fd] = NULL;
			__poller_del_fd(node->data.fd, node->event, poller);
		}

		node->error = 0;
		node->state = PR_ST_STOPPED;
		free(node->res);
		poller->cb((struct poller_result *)node, poller->ctx);
	}

	pthread_mutex_unlock(&poller->mutex);
}

