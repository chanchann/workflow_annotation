/*
  Copyright (c) 2020 Sogou, Inc.

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

/*
 * This message queue originates from the project of Sogou C++ Workflow:
 * https://github.com/sogou/workflow
 *
 * The idea of this implementation is quite simple and abvious. When the
 * get_list is not empty, consumer takes a message. Otherwise the consumer
 * waits till put_list is not empty, and swap two lists. This method performs
 * well when the queue is very busy, and the number of consumers is big.
 */

#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
#include "msgqueue.h"

// 消息队列就是个单链表
// 此处有两个链表，高效swap使用
struct __msgqueue
{
	size_t msg_max;
	size_t msg_cnt;
	int linkoff;
	int nonblock;
	void *head1;     // get_list   
	void *head2;     // put_list
	// 两个list，高效率，一个在get_list拿，一个在put_list放
	// 如果get_list空，如果put_list放了的话，那么swap一下就可了，O(1),非常高效，而且互不干扰
	void **get_head;	
	void **put_head;
	void **put_tail;
	pthread_mutex_t get_mutex;
	pthread_mutex_t put_mutex;
	pthread_cond_t get_cond;
	pthread_cond_t put_cond;
};

void msgqueue_set_nonblock(msgqueue_t *queue)
{
	queue->nonblock = 1;
	pthread_mutex_lock(&queue->put_mutex);
	pthread_cond_signal(&queue->get_cond);
	pthread_cond_broadcast(&queue->put_cond);
	pthread_mutex_unlock(&queue->put_mutex);
}

void msgqueue_set_block(msgqueue_t *queue)
{
	queue->nonblock = 0;
}

static size_t __msgqueue_swap(msgqueue_t *queue)
{
	void **get_head = queue->get_head;
	size_t cnt;
	// 将get_head切换好，因为就算put在加，put_head也不会变, 所以不需要加锁
	queue->get_head = queue->put_head;  

	pthread_mutex_lock(&queue->put_mutex);

	// 如果put_list也没有消息且为阻塞态，那么就wait等到放进来消息
	while (queue->msg_cnt == 0 && !queue->nonblock)
		pthread_cond_wait(&queue->get_cond, &queue->put_mutex);

	cnt = queue->msg_cnt;  
	// 如果cnt大于最大接收的msg，那么通知put
	// todo : why do this?
	if (cnt > queue->msg_max - 1)
		pthread_cond_broadcast(&queue->put_cond);

	queue->put_head = get_head;    // put_list就交换设置到get_list那个地方了  
	queue->put_tail = get_head;

	// put_list清0了
	queue->msg_cnt = 0;     // todo : 此处为何设置为0

	pthread_mutex_unlock(&queue->put_mutex);
	return cnt;
}

/**
 * @brief 就是把msg串到queue后面去
 * msqqueue是epoll消息回来之后，以网络线程作为生产者往queue里放、执行线程作为消费者从queue里拿数据，从而做到线程互不干扰
 * 
 * @param msg 
 * @param queue 
 */
void msgqueue_put(void *msg, msgqueue_t *queue)
{
	// 这里转char* 是因为，void* 不能加减运算，但char* 可以
	void **link = (void **)((char *)msg + queue->linkoff);
	/*
	this->queue = msgqueue_create(4096, sizeof (struct poller_result));
	初始化的时候把linkoff大小设置成了sizeof (struct poller_result)
	*/
	// msg头部偏移linkoff字节，是链表指针的位置。使用者需要留好空间。这样我们就无需再malloc和free了
	// 我们就是把一个个的struct poller_result 串起来
	*link = NULL; 

	pthread_mutex_lock(&queue->put_mutex);
	
	// 当收到的cnt大于最大限制 且 阻塞mode， 那么wait在这
	while (queue->msg_cnt > queue->msg_max - 1 && !queue->nonblock)
		pthread_cond_wait(&queue->put_cond, &queue->put_mutex);

	*queue->put_tail = link;  // 把 link串到链尾
	queue->put_tail = link;   // 然后把这个指针移过来

	queue->msg_cnt++;
	pthread_mutex_unlock(&queue->put_mutex);

	pthread_cond_signal(&queue->get_cond);
}

void *msgqueue_get(msgqueue_t *queue)
{
	void *msg;

	pthread_mutex_lock(&queue->get_mutex);

	// 如果get_list有消息
	// 若get_list无消息了，那么看看put_list有没有，如果有，swap一下即可
	if (*queue->get_head || __msgqueue_swap(queue) > 0)
	{
		
		msg = (char *)*queue->get_head - queue->linkoff;
		*queue->get_head = *(void **)*queue->get_head;
	}
	else
	{
		msg = NULL;
		errno = ENOENT;
	}

	pthread_mutex_unlock(&queue->get_mutex);
	return msg;
}

/**
 * @brief 分配空间+初始化
 * 
 * @param maxlen 
 * @param linkoff 
 * @return msgqueue_t* 
 */
msgqueue_t *msgqueue_create(size_t maxlen, int linkoff)
{
	msgqueue_t *queue = (msgqueue_t *)malloc(sizeof (msgqueue_t));
	int ret;

	if (!queue)
		return NULL;

	ret = pthread_mutex_init(&queue->get_mutex, NULL);
	if (ret == 0)
	{
		ret = pthread_mutex_init(&queue->put_mutex, NULL);
		if (ret == 0)
		{
			ret = pthread_cond_init(&queue->get_cond, NULL);
			if (ret == 0)
			{
				ret = pthread_cond_init(&queue->put_cond, NULL);
				if (ret == 0)
				{
					queue->msg_max = maxlen;
					queue->linkoff = linkoff;
					queue->head1 = NULL;
					queue->head2 = NULL;
					queue->get_head = &queue->head1;
					queue->put_head = &queue->head2;
					queue->put_tail = &queue->head2;
					queue->msg_cnt = 0;
					queue->nonblock = 0;
					return queue;
				}

				pthread_cond_destroy(&queue->get_cond);
			}

			pthread_mutex_destroy(&queue->put_mutex);
		}

		pthread_mutex_destroy(&queue->get_mutex);
	}

	errno = ret;
	free(queue);
	return NULL;
}

void msgqueue_destroy(msgqueue_t *queue)
{
	pthread_cond_destroy(&queue->put_cond);
	pthread_cond_destroy(&queue->get_cond);
	pthread_mutex_destroy(&queue->put_mutex);
	pthread_mutex_destroy(&queue->get_mutex);
	free(queue);
}

