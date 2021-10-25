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
#include <vector>
#include <atomic>
#include "URIParser.h"
#include "StringUtil.h"
#include "WFDnsClient.h"

using namespace protocol;

using DnsCtx = std::function<void (WFDnsTask *)>;
using ComplexTask = WFComplexClientTask<DnsRequest, DnsResponse, DnsCtx>;

class DnsParams
{
public:
	struct dns_params
	{
		std::vector<ParsedURI> uris;
		std::vector<std::string> search_list;
		int ndots;
		int attempts;
		bool rotate;
	};

public:
	DnsParams()
	{
		this->ref = new std::atomic<size_t>(1);
		this->params = new dns_params();
	}

	DnsParams(const DnsParams& p)
	{
		this->ref = p.ref;
		this->params = p.params;
		this->incref();
	}

	DnsParams& operator=(const DnsParams& p)
	{
		if (this != &p)
		{
			this->decref();
			this->ref = p.ref;
			this->params = p.params;
			this->incref();
		}
		return *this;
	}

	~DnsParams() { this->decref(); }

	const dns_params *get_params() const { return this->params; }
	dns_params *get_params() { return this->params; }

private:
	void incref() { (*this->ref)++; }
	void decref()
	{
		if (--*this->ref == 0)
		{
			delete this->params;
			delete this->ref;
		}
	}

private:
	dns_params *params;
	std::atomic<size_t> *ref;
};

enum
{
	DNS_STATUS_TRY_ORIGIN_DONE = 0,
	DNS_STATUS_TRY_ORIGIN_FIRST = 1,
	DNS_STATUS_TRY_ORIGIN_LAST = 2
};

struct DnsStatus
{
	std::string origin_name;
	std::string current_name;
	size_t next_server;			// next server to try
	size_t last_server;			// last server to try
	size_t next_domain;			// next search domain to try
	int attempts_left;
	int try_origin_state;
};

/**
 * @brief 获取字符串中 . 的个数
 * 
 * @param s 
 * @return int 
 */
static int __get_ndots(const std::string& s)
{
	int ndots = 0;
	for (size_t i = 0; i < s.size(); i++)
		ndots += s[i] == '.';
	return ndots;
}

// todo : 仔细画流程图分析这个判断
static bool __has_next_name(const DnsParams::dns_params *p,
							struct DnsStatus *s)
{
	if (s->try_origin_state == DNS_STATUS_TRY_ORIGIN_FIRST)
	{
		s->current_name = s->origin_name;
		s->try_origin_state = DNS_STATUS_TRY_ORIGIN_DONE;
		return true;
	}

	if (s->next_domain < p->search_list.size())
	{
		s->current_name = s->origin_name;
		s->current_name.push_back('.');
		s->current_name.append(p->search_list[s->next_domain]);

		s->next_domain++;
		return true;
	}

	if (s->try_origin_state == DNS_STATUS_TRY_ORIGIN_LAST)
	{
		s->current_name = s->origin_name;
		s->try_origin_state = DNS_STATUS_TRY_ORIGIN_DONE;
		return true;
	}

	return false;
}

static void __callback_internal(WFDnsTask *task, const DnsParams& params,
								struct DnsStatus& s)
{
	ComplexTask *ctask = static_cast<ComplexTask *>(task);
	int state = task->get_state();
	DnsRequest *req = task->get_req();
	DnsResponse *resp = task->get_resp();
	const auto *p = params.get_params();
	int rcode = resp->get_rcode();  // dns_parser.h 中见 RCODE 各个值的含义

	bool try_next_server = state != WFT_STATE_SUCCESS ||
						   rcode == DNS_RCODE_SERVER_FAILURE ||
						   rcode == DNS_RCODE_NOT_IMPLEMENTED ||
						   rcode == DNS_RCODE_REFUSED;
	bool try_next_name = rcode == DNS_RCODE_FORMAT_ERROR ||
						 rcode == DNS_RCODE_NAME_ERROR ||
						 resp->get_ancount() == 0;
	/*
	ANCOUNT     an unsigned 16 bit integer specifying the number of
                resource records in the answer section.
	*/

	if (try_next_server)
	{
		if (s.last_server == s.next_server)
			s.attempts_left--;
		if (s.attempts_left <= 0)
			return;

		s.next_server = (s.next_server + 1) % p->uris.size();
		ctask->set_redirect(p->uris[s.next_server]);
		return;
	}

	if (try_next_name && __has_next_name(p, &s))
	{
		req->set_question_name(s.current_name.c_str());
		ctask->set_redirect(p->uris[s.next_server]);
		return;
	}
}

/**
 * @brief 如果只传url，则前天的填入默认参数
 * 
 * @param url 
 * @return int 
 */
int WFDnsClient::init(const std::string& url)
{
	return this->init(url, "", 1, 2, false);
}

int WFDnsClient::init(const std::string& url, const std::string& search_list,
					  int ndots, int attempts, bool rotate)
{
	std::vector<std::string> hosts;
	std::vector<ParsedURI> uris;
	std::string host;
	ParsedURI uri;

	id = 0;
	// 因为可能设置多个nameserver的ip，我们把切开
	// 比如 0.0.0.0,0.0.0.1:1,dns://0.0.0.2,dnss://0.0.0.3
	// 切成 [0.0.0.0] [0.0.0.1] [dns://0.0.0.2] [dnss://0.0.0.3]
	hosts = StringUtil::split_filter_empty(url, ',');
	for (size_t i = 0; i < hosts.size(); i++)
	{
		host = hosts[i];
		// 这里就是如果是裸的"xxx.xxx.xx" 就给他加上"dns://"
		// 切成 [dns://0.0.0.0] [dns://0.0.0.1] [dns://0.0.0.2] [dnss://0.0.0.3]
		if (strncasecmp(host.c_str(), "dns://", 6) != 0 &&
			strncasecmp(host.c_str(), "dnss://", 7) != 0)
		{
			host = "dns://" + host;
		}
		// 解析这个"dns://xxx.xxx.xxx"
		if (URIParser::parse(host, uri) != 0)
			return -1;

		uris.emplace_back(std::move(uri));
	}

	if (uris.empty() || ndots < 0 || attempts < 1)
	{
		// EINVAL表示 无效的参数，即为 invalid argument ，包括参数值、类型或数目无效等
		errno = EINVAL; 
		return -1;
	}
	
	// 设置params参数
	this->params = new DnsParams;
	DnsParams::dns_params *q = ((DnsParams *)(this->params))->get_params();
	q->uris = std::move(uris);
	q->search_list = StringUtil::split_filter_empty(search_list, ',');  
	q->ndots = ndots > 15 ? 15 : ndots;  // ndots 最大为15
	q->attempts = attempts > 5 ? 5 : attempts;  // attempts 最大为5，都是man手册里的规定
	q->rotate = rotate;

	return 0;
}

void WFDnsClient::deinit()
{
	delete (DnsParams *)(this->params);
	this->params = NULL;
}

WFDnsTask *WFDnsClient::create_dns_task(const std::string& name,
										dns_callback_t callback)
{
	DnsParams::dns_params *p = ((DnsParams *)(this->params))->get_params();
	struct DnsStatus status;
	size_t next_server;
	WFDnsTask *task;
	DnsRequest *req;

	// 如果是rotate则round robin 去负载均衡, 这里的atmic<int> id 就是这个作用
	next_server = p->rotate ? this->id++ % p->uris.size() : 0;

	status.origin_name = name;     
	status.next_domain = 0;
	status.attempts_left = p->attempts;
	status.try_origin_state = DNS_STATUS_TRY_ORIGIN_FIRST;

	// 如果是以 '.' 结尾
	// rfc 1034 
	// https://www.ietf.org/rfc/rfc1034.txt
	/*
	Since a complete domain name ends with the root label, this leads to a
	printed form which ends in a dot.  We use this property to distinguish between:

	- a character string which represents a complete domain name
		(often called "absolute").  For example, "poneria.ISI.EDU."

	- a character string that represents the starting labels of a
		domain name which is incomplete, and should be completed by
		local software using knowledge of the local domain (often
		called "relative").  For example, "poneria" used in the
		ISI.EDU domain.
	*/
	if (!name.empty() && name.back() == '.')   
		status.next_domain = p->uris.size();   // todo : 为何此处next_domain就设置为uri的个数 
	else if (__get_ndots(name) < p->ndots)   // 如果name中 . 的个数小于我们设置的ndots 阈值
		status.try_origin_state = DNS_STATUS_TRY_ORIGIN_LAST;
	// https://tizeen.github.io/2019/02/27/DNS%E4%B8%AD%E7%9A%84ndots/
	// 在所有查询中，如果 . 的个数小于 ndots 指定的数，则会根据 search 中配置的列表依次在对应域中查询，
	// 如果没有返回，则最后直接查询域名本身。

	__has_next_name(p, &status);

	// 真正的task还是通过工厂模式来创建，client只是包了这么一层
	task = WFTaskFactory::create_dns_task(p->uris[next_server],
										  0, std::move(callback));
	status.next_server = next_server;  // 选择配的其中一个name_server去查询
	status.last_server = (next_server + p->uris.size() - 1) % p->uris.size();  // 这里是一个环形的，找到相对于此的最后一个server idx

	req = task->get_req();
	// 这里设置了 question section
	// https://en.wikipedia.org/wiki/Domain_Name_System
	// 主要就是包括这三个参数部门，(NAME, TYPE, CLASS)
	// The CLASS of a record is set to IN (for Internet) for common DNS records involving Internet hostnames, servers, or IP addresses.
	//  In addition, the classes Chaos (CH) and Hesiod (HS) exist.[38]
	req->set_question(status.current_name.c_str(), DNS_TYPE_A, DNS_CLASS_IN);
	req->set_rd(1);  // Recursion Desired, indicates if the client means a recursive query	

	ComplexTask *ctask = static_cast<ComplexTask *>(task);
	// todo : 重点梳理ctx
	*ctask->get_mutable_ctx() = std::bind(__callback_internal,
										  std::placeholders::_1, 
										  *(DnsParams *)params, status); 

	return task;
}

