#include "message.h"
#include <cstring>

namespace protocol
{

	int EchoMessage::encode(struct iovec vectors[], int max /*max==8192*/)
	{
		vectors[0].iov_base = &*body_.begin();
		vectors[0].iov_len = bodySize_;

		return 1; /* return the number of vectors used, no more then max. */
	}

	int EchoMessage::append(const void *buf, size_t size)
	{
		if (size > body_.size())
			body_.resize(size);
		SPDLOG_DEBUG("Welcome to spdlog!");
		// void* memcpy( void* dest, const void* src, std::size_t count );
		std::memcpy(static_cast<void *>(&*body_.begin()), buf, size);
		return 1;
	}

	int EchoMessage::set_message_body(const std::string &body)
	{
		body_ = std::move(body);
		bodySize_ = body.size();
		return 0;
	}

	void EchoMessage::get_message_body_nocopy(std::string *body, size_t *size)
	{
		body = &body_;
		*size = bodySize_;
	}

}
