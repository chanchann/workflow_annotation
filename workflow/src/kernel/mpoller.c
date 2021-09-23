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

#include <stddef.h>
#include <stdlib.h>
#include "poller.h"
#include "mpoller.h"

/**
 * @brief 挨个创建/初始poller
 * 
 * @param params 
 * @param mpoller 
 * @return int 
 */
static int __mpoller_create(const struct poller_params *params,
							mpoller_t *mpoller)
{
	unsigned int i;

	for (i = 0; i < mpoller->nthreads; i++)
	{
		// 挨着挨着初始化每一个poller
		mpoller->poller[i] = poller_create(params);
		if (!mpoller->poller[i])  // 当有个不成功就break出去
			break;
	}

	if (i == mpoller->nthreads)
		return 0;  // 都成功则返回0

	while (i > 0)
		poller_destroy(mpoller->poller[--i]);  // 如果不成功，则挨个destroy掉

	return -1;
}

mpoller_t *mpoller_create(const struct poller_params *params, size_t nthreads)
{
	mpoller_t *mpoller;
	size_t size;

	if (nthreads == 0)
		nthreads = 1;   // 默认至少有一个poller线程

	// offsetof : offsetof - offset of a structure member
	// https://man7.org/linux/man-pages/man3/offsetof.3.html
	// 这里的poller是mpoller_t的第二个成员变量

	// Becareful void* here 
	// void * can hold any data pointer, but not function pointers.
	// https://stackoverflow.com/questions/6908686/c-size-of-void
	// 此处  offsetof(mpoller_t, poller) : 第一个元素(nthreads) 大小 (一般member var 考虑 内存对齐问题)
	// 第二个部分是nthreads 个
	// todo: 为何使用 void *
	size = offsetof(mpoller_t, poller) + nthreads * sizeof (void *);  
	mpoller = (mpoller_t *)malloc(size);    
	if (mpoller)  // 如果malloc成功
	{
		mpoller->nthreads = (unsigned int)nthreads;   // 设置第一个元素
		if (__mpoller_create(params, mpoller) >= 0)
			return mpoller;   // create成功

		free(mpoller);  // create失败则释放
	}

	return NULL;
}

int mpoller_start(mpoller_t *mpoller)
{
	size_t i;

	for (i = 0; i < mpoller->nthreads; i++)
	{
		if (poller_start(mpoller->poller[i]) < 0)
			break;
	}

	if (i == mpoller->nthreads)
		return 0;

	while (i > 0)
		poller_stop(mpoller->poller[--i]);

	return -1;
}

void mpoller_stop(mpoller_t *mpoller)
{
	size_t i;

	for (i = 0; i < mpoller->nthreads; i++)
		poller_stop(mpoller->poller[i]);
}

void mpoller_destroy(mpoller_t *mpoller)
{
	size_t i;

	for (i = 0; i < mpoller->nthreads; i++)
		poller_destroy(mpoller->poller[i]);

	free(mpoller);
}

