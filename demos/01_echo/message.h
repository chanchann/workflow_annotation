#ifndef MESSAGE_H
#define MESSAGE_H

#include <workflow/ProtocolMessage.h>
#include <string>
#include <spdlog/spdlog.h>

// todo : -Woverloaded-virtual fix ?

namespace protocol
{

	class EchoMessage : public ProtocolMessage
	{
	private:
		virtual int encode(struct iovec vectors[], int max);
		virtual int append(const void *buf, size_t size);

	public:
		int set_message_body(const std::string &body);
		void get_message_body_nocopy(std::string *body, size_t *size);

	public:
		EchoMessage() = default;
		virtual ~EchoMessage() = default;

	protected:
		std::string body_;
		size_t bodySize_ = 0;
	};

	// request和response类，都是同一种类型的消息。直接using就可以。
	using EchoRequest = EchoMessage;
	using EchoResponse = EchoMessage;
	// 通讯过程中，如果发生重试，response对象会被销毁并重新构造。因此，它最好是一个RAII类。否则处理起来会比较复杂。
}

#endif