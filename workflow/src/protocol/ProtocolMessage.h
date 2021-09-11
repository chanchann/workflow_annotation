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

#ifndef _PROTOCOLMESSAGE_H_
#define _PROTOCOLMESSAGE_H_

#include <errno.h>
#include <stddef.h>
#include <utility>
#include "Communicator.h"

/**
 * @file   ProtocolMessage.h
 * @brief  General Protocol Interface
 */

namespace protocol
{

// 用户自定义协议，需要提供协议的序列化(encode)和反序列化方法(append)，这两个方法都是ProtocolMeessage类的虚函数
// 强烈建议用户实现消息的移动构造和移动赋值（用于std::move()）

/*
关于序列化encode

encode函数在消息被发送之前调用，每条消息只调用一次。
encode函数里，用户需要将消息序列化到一个vector数组，数组元素个数不超过max。目前max的值为8192。
结构体struct iovec定义在请参考系统调用readv和writev。
encode函数正确情况下的返回值在0到max之间，表示消息使用了多少个vector。
如果是UDP协议，请注意总长度不超过64k，并且使用不超过1024个vector（Linux一次writev只能1024个vector）。
UDP协议只能用于client，无法实现UDP server。
encode返回-1表示错误。返回-1时，需要置errno。如果返回值>max，将得到一个EOVERFLOW错误。错误都在callback里得到。
为了性能考虑vector里的iov_base指针指向的内容不会被复制。所以一般指向消息类的成员。
*/

/*
关于反序列化append

append函数在每次收到一个数据块时被调用。因此，每条消息可能会调用多次。
buf和size分别是收到的数据块内容和长度。用户需要把数据内容复制走。

如果实现了append(const void *buf, size_t *size)接口，可以通过修改*size来告诉框架本次消费了多少长度。
收到的size - 消费的size = 剩余的size，剩余的那部分buf会由下一次append被调起时再次收到。
此功能更方便协议解析，当然用户也可以全部复制走自行管理，则无需修改*size。

append函数返回0表示消息还不完整，传输继续。返回1表示消息结束。-1表示错误，需要置errno。
总之append的作用就是用于告诉框架消息是否已经传输结束。不要在append里做复杂的非必要的协议解析。
*/

/*
errno 设置

encode或append返回-1或其它负数都会被理解为失败，需要通过errno来传递错误原因。用户会在callback里得到这个错误。
如果是系统调用或libc等库函数失败（比如malloc）,libc肯定会设置好errno，用户无需再设置。
一些消息不合法的错误是比较常见的，比如可以用EBADMSG，EMSGSIZE分别表示消息内容错误，和消息太大。
用户可以选择超过系统定义errno范围的值来表示一些自定义错误。一般大于256的值是可以用的。
请不要使用负数errno。因为框架内部用了负数来代表SSL错误。
*/

class ProtocolMessage : public CommMessageOut, public CommMessageIn
{
protected:
	// 序列化encode
	virtual int encode(struct iovec vectors[], int max)
	{
		errno = ENOSYS;   // 非法系统调用号
		return -1;
	}

	/* You have to implement one of the 'append' functions, and the first one
	 * with arguement 'size_t *size' is recommmended. */

	/* Argument 'size' indicates bytes to append, and returns bytes used. */
	virtual int append(const void *buf, size_t *size)
	{
		return this->append(buf, *size);
	}

	/* When implementing this one, all bytes are consumed. Cannot support
	 * streaming protocol. */
	virtual int append(const void *buf, size_t size)
	{
		errno = ENOSYS;
		return -1;
	}

public:
	void set_size_limit(size_t limit) { this->size_limit = limit; }
	size_t get_size_limit() const { return this->size_limit; }

public:
	class Attachment
	{
	public:
		virtual ~Attachment() { }
	};

	void set_attachment(Attachment *att) { this->attachment = att; }
	Attachment *get_attachment() const { return this->attachment; }

protected:
	virtual int feedback(const void *buf, size_t size)
	{
		if (this->wrapper)
			return this->wrapper->feedback(buf, size);
		else
			return this->CommMessageIn::feedback(buf, size);
	}

protected:
	size_t size_limit;

private:
	Attachment *attachment;
	ProtocolMessage *wrapper;

public:
	ProtocolMessage()
	{
		this->size_limit = (size_t)-1;
		this->attachment = NULL;
		this->wrapper = NULL;
	}

	virtual ~ProtocolMessage() { delete this->attachment; }

public:
	ProtocolMessage(ProtocolMessage&& msg)
	{
		this->size_limit = msg.size_limit;
		msg.size_limit = (size_t)-1;
		this->attachment = msg.attachment;
		msg.attachment = NULL;
	}

	ProtocolMessage& operator = (ProtocolMessage&& msg)
	{
		if (&msg != this)
		{
			this->size_limit = msg.size_limit;
			msg.size_limit = (size_t)-1;
			delete this->attachment;
			this->attachment = msg.attachment;
			msg.attachment = NULL;
		}

		return *this;
	}

	friend class ProtocolWrapper;
};

class ProtocolWrapper : public ProtocolMessage
{
protected:
	virtual int encode(struct iovec vectors[], int max)
	{
		return this->msg->encode(vectors, max);
	}

	virtual int append(const void *buf, size_t *size)
	{
		return this->msg->append(buf, size);
	}

protected:
	ProtocolMessage *msg;

public:
	ProtocolWrapper(ProtocolMessage *msg)
	{
		msg->wrapper = this;
		this->msg = msg;
	}

public:
	ProtocolWrapper(ProtocolWrapper&& wrapper) :
		ProtocolMessage(std::move(wrapper))
	{
		wrapper.msg->wrapper = this;
		this->msg = wrapper.msg;
		wrapper.msg = NULL;
	}

	ProtocolWrapper& operator = (ProtocolWrapper&& wrapper)
	{
		if (&wrapper != this)
		{
			*(ProtocolMessage *)this = std::move(wrapper);
			wrapper.msg->wrapper = this;
			this->msg = wrapper.msg;
			wrapper.msg = NULL;
		}

		return *this;
	}
};

}

#endif

