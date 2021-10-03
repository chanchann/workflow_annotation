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

#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
#include "list.h"
#include "thrdpool.h"
#include "Executor.h"
#include "logger.h"

struct ExecTaskEntry
{
	struct list_head list;
	ExecSession *session;
	thrdpool_t *thrdpool;
};

/**
 * @brief  就是初始化list和mutex
 * @note   
 * @retval 
 */
int ExecQueue::init()
{
	int ret;
	LOG_TRACE("ExecQueue::init");
	ret = pthread_mutex_init(&this->mutex, NULL);
	if (ret == 0)
	{
		INIT_LIST_HEAD(&this->task_list);
		return 0;
	}

	errno = ret;
	return -1;
}

/**
 * @brief  就是销毁mutex
 * @note   
 * @retval None
 */
void ExecQueue::deinit()
{
	pthread_mutex_destroy(&this->mutex);
}

int Executor::init(size_t nthreads)
{
	LOG_TRACE("Executor::init");
	if (nthreads == 0)
	{
		errno = EINVAL;
		return -1;
	}
	LOG_TRACE("Calculate thread pool create");
	this->thrdpool = thrdpool_create(nthreads, 0);
	if (this->thrdpool)
		return 0;

	return -1;
}

void Executor::deinit()
{
	thrdpool_destroy(Executor::executor_cancel_tasks, this->thrdpool);
}

extern "C" void __thrdpool_schedule(const struct thrdpool_task *, void *,
									thrdpool_t *);

void Executor::executor_thread_routine(void *context)
{
	LOG_TRACE("Executor::executor_thread_routine");
	ExecQueue *queue = (ExecQueue *)context;
	struct ExecTaskEntry *entry;
	ExecSession *session;

	pthread_mutex_lock(&queue->mutex);

	// 取出一个任务
	entry = list_entry(queue->task_list.next, struct ExecTaskEntry, list);

	list_del(&entry->list);
	session = entry->session;

	if (!list_empty(&queue->task_list))
	{
		// 如果任务队列不为空
		// 之前request如果是第一个，需要先去malloc entry
		// 队列后面的就不需要malloc了，直接复用
		struct thrdpool_task task = {
			.routine	=	Executor::executor_thread_routine,
			.context	=	queue
		};
		// 重复利用了这个entry，设置entry->task = task，再次加入线程池中
		__thrdpool_schedule(&task, entry, entry->thrdpool);
	}
	else
		free(entry);

	pthread_mutex_unlock(&queue->mutex);

	session->execute();   // cb执行
	session->handle(ES_STATE_FINISHED, 0);   // 任务完成，设置state，error
}

void Executor::executor_cancel_tasks(const struct thrdpool_task *task)
{
	ExecQueue *queue = (ExecQueue *)task->context;
	struct ExecTaskEntry *entry;
	struct list_head *pos, *tmp;
	ExecSession *session;

	list_for_each_safe(pos, tmp, &queue->task_list)
	{
		entry = list_entry(pos, struct ExecTaskEntry, list);
		list_del(pos);
		session = entry->session;
		free(entry);

		session->handle(ES_STATE_CANCELED, 0);
	}
}

int Executor::request(ExecSession *session, ExecQueue *queue)
{
	LOG_TRACE("Executor::request");
	struct ExecTaskEntry *entry;

	session->queue = queue;   // 给session的queue设置上
	// 实例化一个entry
	entry = (struct ExecTaskEntry *)malloc(sizeof (struct ExecTaskEntry));
	if (entry)
	{
		entry->session = session;   // entry对应的session，session才是需要去执行的(session->execute())
		entry->thrdpool = this->thrdpool;  // entry对应的线程池

		pthread_mutex_lock(&queue->mutex);

		list_add_tail(&entry->list, &queue->task_list);  // task加入执行队列中
		LOG_TRACE("add task to task_list");
		
		if (queue->task_list.next == &entry->list)    
		// 如果这是第一个任务，需要去thrdpool_schedule(第一次调用需要malloc buf(entry(struct __thrdpool_task_entry)))
		// 后面就复用entry(struct __thrdpool_task_entry)了
		{
			// executor_thread_routine 就是取出queue中的任务去运行
			// 我们把包装成一个task，就是为了分发给各个线程
			struct thrdpool_task task = {
				.routine	=	Executor::executor_thread_routine,
				.context	=	queue
			};
			// 将task放入线程池(第一次用就调用thrdpool_schedule，之后就是__thrdpool_schedule)
			if (thrdpool_schedule(&task, this->thrdpool) < 0)
			{
				list_del(&entry->list);
				free(entry);
				entry = NULL;
			}
		}

		pthread_mutex_unlock(&queue->mutex);
	}

	return -!entry;
}

