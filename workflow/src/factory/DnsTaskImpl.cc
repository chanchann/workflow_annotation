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

  Author: Liu Kai (liukaidx@sogou-inc.com)
*/

#include <string>
#include "WFTaskError.h"
#include "WFTaskFactory.h"
#include "DnsMessage.h"

using namespace protocol;

#define DNS_KEEPALIVE_DEFAULT	(60 * 1000)

/**********Client**********/

class ComplexDnsTask : public WFComplexClientTask<DnsRequest, DnsResponse,
							  std::function<void (WFDnsTask *)>>
{

	// /* Structure to contain information about address of a service provider.  */
	// struct addrinfo
	// {
	// int ai_flags;			/* Input flags.  */
	// int ai_family;		/* Protocol family for socket.  */
	// int ai_socktype;		/* Socket type.  */
	// int ai_protocol;		/* Protocol for socket.  */
	// socklen_t ai_addrlen;		/* Length of socket address.  */
	// struct sockaddr *ai_addr;	/* Socket address for socket.  */
	// char *ai_canonname;		/* Canonical name for service location.  */
	// struct addrinfo *ai_next;	/* Pointer to next in list.  */
	// };
	static struct addrinfo hints;

public:
	ComplexDnsTask(int retry_max, dns_callback_t&& cb):
		WFComplexClientTask(retry_max, std::move(cb))
	{
		// dns 是建立在 UDP 之上的应用层协议
		// WFComplexClientTask中默认TT_TCP
		this->set_transport_type(TT_UDP);
	}

protected:
	virtual CommMessageOut *message_out();
	virtual CommMessageIn *message_in();
	virtual bool init_success();
	virtual bool finish_once();

private:
	bool need_redirect();
};

// 初始化static member
struct addrinfo ComplexDnsTask::hints =
{
	.ai_flags     = AI_NUMERICSERV | AI_NUMERICHOST,
	.ai_family    = AF_UNSPEC,
	.ai_socktype  = SOCK_STREAM,
	.ai_protocol  = 0,
	.ai_addrlen   = 0,
	.ai_addr      = NULL,
	.ai_canonname = NULL,
	.ai_next      = NULL
};

CommMessageOut *ComplexDnsTask::message_out()
{
	DnsRequest *req = this->get_req();
	DnsResponse *resp = this->get_resp();
	TransportType type = this->get_transport_type();

	/* Set these field every time, in case of reconstruct on redirect */
	resp->set_request_id(req->get_id());
	resp->set_request_name(req->get_question_name());
	req->set_single_packet(type == TT_UDP);
	resp->set_single_packet(type == TT_UDP);

	return this->WFClientTask::message_out();
}

CommMessageIn *ComplexDnsTask::message_in()
{
	return this->WFClientTask::message_in();
}

/**
 * @brief client init的时候调用
 * @note   
 * @retval 
 */
bool ComplexDnsTask::init_success()
{

	if (uri_.scheme && strcasecmp(uri_.scheme, "dnss") == 0)
		// 如果scheme是dnss
		this->WFComplexClientTask::set_transport_type(TT_TCP_SSL);
	else if (uri_.scheme && strcasecmp(uri_.scheme, "dns") != 0)
	{
		// 如果既不是dnss, 又不是dns，那么是错的
		this->state = WFT_STATE_TASK_ERROR;
		this->error = WFT_ERR_URI_SCHEME_INVALID;
		return false;
	}

	if (!this->route_result_.request_object)
	{
		TransportType type = this->get_transport_type();
		struct addrinfo *addr;
		int ret;
		
		ret = getaddrinfo(uri_.host, uri_.port, &hints, &addr);
		if (ret != 0)
		{
			this->state = WFT_STATE_TASK_ERROR;
			this->error = WFT_ERR_URI_PARSE_FAILED;
			return false;
		}

		auto *ep = &WFGlobal::get_global_settings()->dns_server_params;
		ret = WFGlobal::get_route_manager()->get(type, addr, info_, ep,
												 uri_.host, route_result_);
		freeaddrinfo(addr);
		if (ret < 0)
		{
			this->state = WFT_STATE_SYS_ERROR;
			this->error = errno;
			return false;
		}
	}

	return true;
}

bool ComplexDnsTask::finish_once()
{
	if (this->state == WFT_STATE_SUCCESS)
	{
		if (need_redirect())
			this->set_redirect(uri_);
		else if (this->state != WFT_STATE_SUCCESS)
			this->disable_retry();
	}

	/* If retry times meet retry max and there is no redirect,
	 * we ask the client for a retry or redirect.
	 */
	if (retry_times_ == retry_max_ && !redirect_ && *this->get_mutable_ctx())
	{
		/* Reset type to UDP before a client redirect. */
		this->set_transport_type(TT_UDP);
		(*this->get_mutable_ctx())(this);
	}

	return true;
}

bool ComplexDnsTask::need_redirect()
{
	DnsResponse *client_resp = this->get_resp();
	TransportType type = this->get_transport_type();

	if (type == TT_UDP && client_resp->get_tc() == 1)
	{
		this->set_transport_type(TT_TCP);
		return true;
	}

	return false;
}

/**********Client Factory**********/

// 工厂模式跳开client去创建dns task
// 相对于client，多了一个retry_max
WFDnsTask *WFTaskFactory::create_dns_task(const std::string& url,
										  int retry_max,
										  dns_callback_t callback)
{
	ParsedURI uri;

	URIParser::parse(url, uri);
	return WFTaskFactory::create_dns_task(uri, retry_max, std::move(callback));
}

WFDnsTask *WFTaskFactory::create_dns_task(const ParsedURI& uri,
										  int retry_max,
										  dns_callback_t callback)
{
	ComplexDnsTask *task = new ComplexDnsTask(retry_max, std::move(callback));
	const char *name;

	// todo : 此处为什么这样设置
	if (uri.path && uri.path[0] && uri.path[1])
		name = uri.path + 1;
	else  // 无路径或者路径为 "/"，则设置为 "."
		name = ".";

	DnsRequest *req = task->get_req();
	/*
	https://stackoverflow.com/questions/34841206/why-is-the-content-of-qname-field-not-the-original-domain-in-a-dns-message
	*/
	req->set_question(name, DNS_TYPE_A, DNS_CLASS_IN);

	task->init(uri);
	task->set_keep_alive(DNS_KEEPALIVE_DEFAULT);
	return task;
}

