/*
  Copyright (c) 2021 Sogou, Inc.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  Authors: Li Yingxin (liyingxin@sogou-inc.com)
           Xie Han (xiehan@sogou-inc.com)
*/

#ifndef _WFRESOURCEPOOL_H_
#define _WFRESOURCEPOOL_H_

#include <mutex>
#include "list.h"
#include "WFTask.h"

class WFResourcePool
{
public:
	WFConditional *get(SubTask *task, void **resbuf);
	void post(void *res);

public:
	struct Data
	{
		void *pop() { return this->pool->pop(); }
		void push(void *res) { this->pool->push(res); }

		void **res;  // void* 的数组, 管理资源池中的资源
		long value;   // 这里是剩余资源的大小
		size_t index;
		struct list_head wait_list;   // 把等待的conditional task串成链表
		std::mutex mutex;
		WFResourcePool *pool;
	};

private:
	virtual void *pop()
	{
		LOG_TRACE("WFResourcePool:: pop");
		return this->data.res[this->data.index++];
	}

	virtual void push(void *res)
	{
		LOG_TRACE("WFResourcePool:: push");
		this->data.res[--this->data.index] = res;
	}

private:
	struct Data data;

private:
	void create(size_t n);

public:
	WFResourcePool(void *const *res, size_t n);
	WFResourcePool(size_t n);
	virtual ~WFResourcePool() { delete []this->data.res; }
};

#endif

