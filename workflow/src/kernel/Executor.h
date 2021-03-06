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

#ifndef _EXECUTOR_H_
#define _EXECUTOR_H_

#include <stddef.h>
#include <pthread.h>
#include "list.h"
#include "logger.h"

// 看名字就知道是个执行队列
class ExecQueue
{
public:
	int init();  
	void deinit();

private:
	struct list_head task_list;
	pthread_mutex_t mutex;

public:
	ExecQueue() { LOG_TRACE("ExecQueue creator"); }
	virtual ~ExecQueue() { }
	friend class Executor;
};

#define ES_STATE_FINISHED	0
#define ES_STATE_ERROR		1
#define ES_STATE_CANCELED	2

class ExecSession
{
private:
	virtual void execute() = 0;
	virtual void handle(int state, int error) = 0;

protected:
	ExecQueue *get_queue() { return this->queue; }

private:
	ExecQueue *queue;  

public:
	ExecSession() { LOG_TRACE("ExecSession creator"); }
	virtual ~ExecSession() { }
	friend class Executor;  // Executor可以直接access这里的private queue
};

class Executor
{
public:
	int init(size_t nthreads);  // 创建线程吹
	void deinit();  

	int request(ExecSession *session, ExecQueue *queue);

private:
	struct __thrdpool *thrdpool;

private:
	static void executor_thread_routine(void *context);
	static void executor_cancel_tasks(const struct thrdpool_task *task);

public:
	Executor() { LOG_TRACE("Executor creator"); }
	virtual ~Executor() { }
};

#endif

