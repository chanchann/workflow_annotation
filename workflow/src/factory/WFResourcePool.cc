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

#include <string.h>
#include "list.h"
#include "WFTask.h"
#include "WFResourcePool.h"

class __WFConditional : public WFConditional
{
public:
	struct list_head list;
	struct WFResourcePool::Data *data;

public:
	virtual void dispatch();
	virtual void signal(void *res) { LOG_TRACE("__WFConditional::signal"); }

public:
	__WFConditional(SubTask *task, void **resbuf,
					struct WFResourcePool::Data *data) :
		WFConditional(task, resbuf)
	{
		LOG_TRACE("__WFConditional creator");
		this->data = data;
	}
};

void __WFConditional::dispatch()
{
	LOG_TRACE("__WFConditional::dispatch");
	struct WFResourcePool::Data *data = this->data;

	data->mutex.lock();
	if (--data->value >= 0)
		this->WFConditional::signal(data->pop());
	else
		list_add_tail(&this->list, &data->wait_list);

	data->mutex.unlock();
	this->WFConditional::dispatch();
}

WFConditional *WFResourcePool::get(SubTask *task, void **resbuf)
{
	LOG_TRACE("WFResourcePool::get");
	return new __WFConditional(task, resbuf, &this->data);
}

void WFResourcePool::create(size_t n)
{
	LOG_TRACE("WFResourcePool::create");
	this->data.res = new void *[n];   // 就是创建n个void * 的数组
	this->data.value = n;   // 资源初始化大小为n
	this->data.index = 0;   
	INIT_LIST_HEAD(&this->data.wait_list);
	this->data.pool = this;
}

WFResourcePool::WFResourcePool(void *const *res, size_t n)
{
	LOG_TRACE("WFResourcePool creator");
	this->create(n);
	memcpy(this->data.res, res, n * sizeof (void *));
}

WFResourcePool::WFResourcePool(size_t n)
{
	LOG_TRACE("WFResourcePool creator");
	this->create(n);
	memset(this->data.res, 0, n * sizeof (void *));
}

void WFResourcePool::post(void *res)
{
	LOG_TRACE("WFResourcePool post");
	struct WFResourcePool::Data *data = &this->data;
	WFConditional *cond;

	data->mutex.lock();
	if (++data->value <= 0)   // 如果资源池排队很多，已经大大亏空了，我们放一个资源回去，就得有一个队列中的任务来执行
	{
		// 取出cond来执行
		cond = list_entry(data->wait_list.next, __WFConditional, list);
		list_del(data->wait_list.next);
	}
	else
	{
		// 如果资源过剩，就把资源放回去就完了
		cond = NULL;
		this->push(res); 
	}

	data->mutex.unlock();
	if (cond)
		cond->WFConditional::signal(res);
}

