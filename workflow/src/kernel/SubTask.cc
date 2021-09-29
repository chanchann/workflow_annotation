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

#include "SubTask.h"

/**
 * @brief 不论是并行还是串行，任务的分发都是顺序的，只不过分发到不同的request
 * 如dns dispatch到WFRouterTask::dispatch，
 * http task dispatch到WFComplexClientTask::dispatch,进而Communicator::request
 * 但这个过程是通过ParallelTask::dispatch()顺序依次分发的
 * 另外从服务端接收到数据，放进msgqueue,网络线程池触发handle,每次都必须调用subtask_done，
 * 最终调用用户注册的回掉函数，如果是基于此，并行处理的主要机制其实就是利用queue的特性，多个线程调用各自的注册的回掉函数；
 * 
 */
void SubTask::subtask_done()
{
	SubTask *cur = this;
	ParallelTask *parent;
	SubTask **entry;

	while (1)
	{
		// 任何的Task都是parellel的subTask
		parent = cur->parent;
		entry = cur->entry;
		cur = cur->done();    // 调用任务回调函数
		if (cur)   // 返回了串行的下一个task
		{
			cur->parent = parent;
			cur->entry = entry;
			if (parent)
				*entry = cur;

			cur->dispatch();  // 不同任务分发至不同的处理请求
		}
		else if (parent)   // 如果没有下一个任务了，就往上走
		{
			if (__sync_sub_and_fetch(&parent->nleft, 1) == 0)
			{
				cur = parent;
				continue;
			}
		}

		break;
	}
}

void ParallelTask::dispatch()
{
	SubTask **end = this->subtasks + this->subtasks_nr;
	SubTask **p = this->subtasks;

	this->nleft = this->subtasks_nr;
	if (this->nleft != 0)
	{
		do
		{
			(*p)->parent = this;
			(*p)->entry = p;
			(*p)->dispatch();
		} while (++p != end);
	}
	else
		this->subtask_done();
}

