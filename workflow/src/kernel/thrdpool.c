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
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include "list.h"
#include "thrdpool.h"
#include "logger.h"
struct __thrdpool
{
	struct list_head task_queue;
	size_t nthreads;
	size_t stacksize;
	pthread_t tid;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	pthread_key_t key;
	pthread_cond_t *terminate;
};

struct __thrdpool_task_entry
{
	struct list_head list;
	struct thrdpool_task task;
};

static pthread_t __zero_tid;

/**
 * @brief  线程池的消费者，在创建线程池的时候，当成线程的callback运行起来
 * @note   
 * @param  *arg: 
 * @retval None
 */
static void *__thrdpool_routine(void *arg)
{
	LOG_TRACE("__thrdpool_routine");
	thrdpool_t *pool = (thrdpool_t *)arg;
	struct list_head **pos = &pool->task_queue.next;  // 第一个任务
	/*
	struct __thrdpool_task_entry
	{
		struct list_head list;    // 用来串起来thrdpool_task
		struct thrdpool_task task;
	};
	*/
	struct __thrdpool_task_entry *entry;
	void (*task_routine)(void *);
	void *task_context;
	pthread_t tid;

	// todo : 此处为什么要这样设置
	pthread_setspecific(pool->key, pool);   // 此处把pool搞成thread_local了
	while (1)
	{
		pthread_mutex_lock(&pool->mutex);
		// 这个队列是线程池公共的队列，所以需要加锁
		
		// 此处是消费者行为，如果没停止，且任务队列没任务，就wait在这
		while (!pool->terminate && list_empty(&pool->task_queue))
			pthread_cond_wait(&pool->cond, &pool->mutex);

		if (pool->terminate)  // 停止了就出去
			break;

		// 我们取出第一个这个task
		entry = list_entry(*pos, struct __thrdpool_task_entry, list);
		list_del(*pos); 

		pthread_mutex_unlock(&pool->mutex);

		/*
		struct thrdpool_task
		{
			void (*routine)(void *);
			void *context;
		}; 
		一个task，包含了需要作什么，还需要一个上下文(task运行需要的一些参数)
		*/
		task_routine = entry->task.routine;
		task_context = entry->task.context;
		free(entry);

		// 此处是执行
		task_routine(task_context);

		if (pool->nthreads == 0)
		{
			/* Thread pool was destroyed by the task. */
			free(pool);
			return NULL;
		}
	}

	/* One thread joins another. Don't need to keep all thread IDs. */
	tid = pool->tid;
	pool->tid = pthread_self(); 
	// todo : 退出细节还需要画画图
	if (--pool->nthreads == 0)
		pthread_cond_signal(pool->terminate);

	pthread_mutex_unlock(&pool->mutex);   // todo: 这里如何多了个unlock，之前又是哪里lock的呢，理一下这个

	// todo : 此处__zero_tid何用
	if (memcmp(&tid, &__zero_tid, sizeof (pthread_t)) != 0)
		pthread_join(tid, NULL);

	return NULL;
}

/**
 * @brief  初始化lock相关，就是mutex，conditional_variable
 * @note   
 * @param  *pool: 
 * @retval 
 */
static int __thrdpool_init_locks(thrdpool_t *pool)
{
	int ret;

	ret = pthread_mutex_init(&pool->mutex, NULL);
	if (ret == 0)
	{
		ret = pthread_cond_init(&pool->cond, NULL);
		if (ret == 0)
			return 0;

		pthread_mutex_destroy(&pool->mutex);
	}

	errno = ret;
	return -1;
}

static void __thrdpool_destroy_locks(thrdpool_t *pool)
{
	pthread_mutex_destroy(&pool->mutex);
	pthread_cond_destroy(&pool->cond);
}

static void __thrdpool_terminate(int in_pool, thrdpool_t *pool)
{
	pthread_cond_t term = PTHREAD_COND_INITIALIZER;

	pthread_mutex_lock(&pool->mutex);
	pool->terminate = &term;
	pthread_cond_broadcast(&pool->cond);

	if (in_pool)
	{
		/* Thread pool destroyed in a pool thread is legal. */
		pthread_detach(pthread_self());
		pool->nthreads--;
	}

	while (pool->nthreads > 0)
		pthread_cond_wait(&term, &pool->mutex);

	pthread_mutex_unlock(&pool->mutex);
	if (memcmp(&pool->tid, &__zero_tid, sizeof (pthread_t)) != 0)
		pthread_join(pool->tid, NULL);
}

static int __thrdpool_create_threads(size_t nthreads, thrdpool_t *pool)
{
	pthread_attr_t attr;
	pthread_t tid;
	int ret;

	ret = pthread_attr_init(&attr);
	if (ret == 0)
	{
		if (pool->stacksize)
			pthread_attr_setstacksize(&attr, pool->stacksize);

		while (pool->nthreads < nthreads)
		{
			ret = pthread_create(&tid, &attr, __thrdpool_routine, pool);
			if (ret == 0)
				pool->nthreads++;
			else
				break;
		}

		pthread_attr_destroy(&attr);
		if (pool->nthreads == nthreads)
			return 0;

		__thrdpool_terminate(0, pool);
	}

	errno = ret;
	return -1;
}

/**
 * @brief  线程池的创建
 * @note   
 * @param  nthreads: 
 * @param  stacksize: 
 * @retval 
 */
thrdpool_t *thrdpool_create(size_t nthreads, size_t stacksize)
{
	thrdpool_t *pool;
	int ret;
	// 1. 分配
	pool = (thrdpool_t *)malloc(sizeof (thrdpool_t));
	if (pool)
	{
		// 2. 初始化
		if (__thrdpool_init_locks(pool) >= 0)
		{
			ret = pthread_key_create(&pool->key, NULL);
			if (ret == 0)
			{
				INIT_LIST_HEAD(&pool->task_queue);
				pool->stacksize = stacksize;
				pool->nthreads = 0;
				memset(&pool->tid, 0, sizeof (pthread_t));
				pool->terminate = NULL;
				if (__thrdpool_create_threads(nthreads, pool) >= 0)
					return pool;

				pthread_key_delete(pool->key);
			}
			else
				errno = ret;

			__thrdpool_destroy_locks(pool);
		}

		free(pool);
	}

	return NULL;
}

/**
 * @brief  这里的线程池调度，其实就是生产者，加入任务到list里，通知消费者来消费
 * @note   
 * @param  *task: 
 * @param  *buf: 这里就是struct __thrdpool_task_entry，为了简便写成void *buf
 * @param  *pool: 
 * @retval None
 */
inline void __thrdpool_schedule(const struct thrdpool_task *task, void *buf,
								thrdpool_t *pool)
{
	LOG_TRACE("__thrdpool_schedule");
	struct __thrdpool_task_entry *entry = (struct __thrdpool_task_entry *)buf;

	entry->task = *task;
	pthread_mutex_lock(&pool->mutex);

	// 把任务队列的任务放入线程池中
	LOG_TRACE("add entry list to pool task Queue");
	list_add_tail(&entry->list, &pool->task_queue);

	pthread_cond_signal(&pool->cond);

	pthread_mutex_unlock(&pool->mutex);
}

/**
 * @brief  就是调用__thrdpool_schedule
 * @note   
 * @param  *task: 
 * @param  *pool: 
 * @retval 
 */
int thrdpool_schedule(const struct thrdpool_task *task, thrdpool_t *pool)
{
	// 这是第一次调用，我们分配buf(entry)，后面我们都不需要再次malloc
	// 靠entry->task = *task 来复用entry了
	LOG_TRACE("thrdpool_schedule");
	void *buf = malloc(sizeof (struct __thrdpool_task_entry));
	if (buf)
	{
		__thrdpool_schedule(task, buf, pool);
		return 0;
	}

	return -1;
}

/**
 * @brief  给线程池扩容
 * @note   
 * @param  *pool: 
 * @retval 
 */
int thrdpool_increase(thrdpool_t *pool)
{
	pthread_attr_t attr;
	pthread_t tid;
	int ret;

	ret = pthread_attr_init(&attr);
	if (ret == 0)
	{
		if (pool->stacksize)
			pthread_attr_setstacksize(&attr, pool->stacksize);

		pthread_mutex_lock(&pool->mutex);
		ret = pthread_create(&tid, &attr, __thrdpool_routine, pool);
		if (ret == 0)
			pool->nthreads++;

		pthread_mutex_unlock(&pool->mutex);
		pthread_attr_destroy(&attr);
		if (ret == 0)
			return 0;
	}

	errno = ret;
	return -1;
}

inline int thrdpool_in_pool(thrdpool_t *pool)
{
	return pthread_getspecific(pool->key) == pool;
}

/**
 * @brief  销毁线程池
 * @note   
 * @retval None
 */
void thrdpool_destroy(void (*pending)(const struct thrdpool_task *),
					  thrdpool_t *pool)
{
	int in_pool = thrdpool_in_pool(pool);
	struct __thrdpool_task_entry *entry;
	struct list_head *pos, *tmp;

	__thrdpool_terminate(in_pool, pool);
	list_for_each_safe(pos, tmp, &pool->task_queue)
	{
		entry = list_entry(pos, struct __thrdpool_task_entry, list);
		list_del(pos);
		if (pending)
			pending(&entry->task);

		free(entry);
	}

	pthread_key_delete(pool->key);
	__thrdpool_destroy_locks(pool);
	if (!in_pool)
		free(pool);
}

