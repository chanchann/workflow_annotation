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

#ifndef _DNS_PARSER_H_
#define _DNS_PARSER_H_

#include <stddef.h>
#include <stdint.h>
#include "list.h"

enum
{
	// 这是DNS协议中定义的各种type
	// https://en.wikipedia.org/wiki/List_of_DNS_record_types

	DNS_TYPE_A = 1,
	DNS_TYPE_NS,
	DNS_TYPE_MD,
	DNS_TYPE_MF,
	DNS_TYPE_CNAME,
	DNS_TYPE_SOA = 6,
	DNS_TYPE_MB,
	DNS_TYPE_MG,
	DNS_TYPE_MR,
	DNS_TYPE_NULL,
	DNS_TYPE_WKS = 11,
	DNS_TYPE_PTR,
	DNS_TYPE_HINFO,
	DNS_TYPE_MINFO,
	DNS_TYPE_MX,
	DNS_TYPE_TXT = 16,

	DNS_TYPE_AAAA = 28,
	DNS_TYPE_SRV = 33,

	DNS_TYPE_AXFR = 252,
	DNS_TYPE_MAILB = 253,
	DNS_TYPE_MAILA = 254,
	DNS_TYPE_ALL = 255
};

enum
{
	DNS_CLASS_IN = 1,  // internet 表示网络，最为常用
	DNS_CLASS_CS,
	DNS_CLASS_CH,
	DNS_CLASS_HS,

	DNS_CLASS_ALL = 255
};

enum
{
	DNS_OPCODE_QUERY = 0,
	DNS_OPCODE_IQUERY,
	DNS_OPCODE_STATUS,
};

enum
{
	/*
RCODE           Response code - this 4 bit field is set as part of
                responses.  The values have the following
                interpretation:

                0               No error condition

                1               Format error - The name server was
                                unable to interpret the query.

                2               Server failure - The name server was
                                unable to process this query due to a
                                problem with the name server.

                3               Name Error - Meaningful only for
                                responses from an authoritative name
                                server, this code signifies that the
                                domain name referenced in the query does
                                not exist.

                4               Not Implemented - The name server does
                                not support the requested kind of query.

                5               Refused - The name server refuses to
                                perform the specified operation for
                                policy reasons.  For example, a name
                                server may not wish to provide the
                                information to the particular requester,
                                or a name server may not wish to perform
                                a particular operation (e.g., zone
	*/
	DNS_RCODE_NO_ERROR = 0,
	DNS_RCODE_FORMAT_ERROR,
	DNS_RCODE_SERVER_FAILURE,
	DNS_RCODE_NAME_ERROR,
	DNS_RCODE_NOT_IMPLEMENTED,
	DNS_RCODE_REFUSED
};

/**
 * dns_header_t is a struct to describe the header of a dns
 * request or response packet, but the byte order is not
 * transformed.
 */

/*
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                      ID                       |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |QR|   Opcode  |AA|TC|RD|RA|   Z    |   RCODE   |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                    QDCOUNT                    |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                    ANCOUNT                    |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                    NSCOUNT                    |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                    ARCOUNT                    |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
*/

/**
 * @id 一个16位的ID，请求时指定这个ID，回应指返回相同的ID，用来标识同一个会话.(demo中有测试)
 * @QR 占1位，0表示请求，1表示响应。
 * @opcode 操作码占4位，0表示标准请求
 * @aa 1 bit，代表授权应答，这个AA只在响应报文中有效，值为 1 时，表示名称服务器是权威服务器;值为 0 时，表示不是权威服务器
 * @tc 截断标志位，值为 1 时，表示响应已超过 512 字节并且已经被截断，只返回前 512 个字节
 * @rd Recursion Desired, indicates if the client means a recursive query
 * @ra 可用递归字段，这个字段只出现在响应报文中。当值为 1 时，表示服务器支持递归查询。
 * @z(zero) 保留字段，在所有的请求和应答报文中，它的值必须为 0。
 * @rcode 这个字段是返回码字段，表示响应的差错状态
 * @qdcount an unsigned 16 bit integer specifying the number of entries in the question section.
 * @ancount an unsigned 16 bit integer specifying the number of resource records in the answer section.
 * @nscount an unsigned 16 bit integer specifying the number of name 
 * 			server resource records in the authority records section.
 * @arcount an unsigned 16 bit integer specifying the number of 
 * 	   		resource records in the additional records section.
 */

/*
note: 这里的 : 是位域，bit field的概念
位域(Bit field)为一种数据结构，可以把数据以位的形式紧凑的储存，并允许程序员对此结构的位进行操作
https://murphypei.github.io/blog/2019/06/struct-bit-field
*/
struct dns_header
{
	uint16_t id;
#if __BYTE_ORDER == __LITTLE_ENDIAN
	uint8_t rd : 1;
	uint8_t tc : 1;
	uint8_t aa : 1;
	uint8_t opcode : 4;
	uint8_t qr : 1;
	uint8_t rcode : 4;
	uint8_t z : 3;
	uint8_t ra : 1;
#elif __BYTE_ORDER == __BIG_ENDIAN
	uint8_t qr : 1;
	uint8_t opcode : 4;
	uint8_t aa : 1;
	uint8_t tc : 1;
	uint8_t rd : 1;
	uint8_t ra : 1;
	uint8_t z : 3;
	uint8_t rcode : 4;
#else
#error endian?
#endif
	uint16_t qdcount;
	uint16_t ancount;
	uint16_t nscount;
	uint16_t arcount;
};


/**
 * @brief 提问部分
 * 
 * 请求包需要至少1个提问条目，回应包也会返回提问条件
 * 
 * qname : QNAME 域名是由点和多个标签组成的，比如http://www.google.com一共有三个标签www, google, com：
 * QNAME正常的存储方式是：标签1长度(1字节)+标签1内容+标签2长度(1字节)+标签2内容+...这样的，最后以\0结尾。
 * e.g 3w6google3com0
 * 还有一种压缩的存储
 * 
 * qtype : 提问的类型
 * 有两个是我们关心的：
 * A 1 (DNS_TYPE_A) IPv4地址 
 * AAAA 28 (DNS_TYPE_AAAA) IPv6地址
 * 
 * QCLASS 提问类别，1表示网络(DNS_CLASS_IN)
 */
/*
                                    1  1  1  1  1  1
      0  1  2  3  4  5  6  7  8  9  0  1  2  3  4  5
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                                               |
    /                     QNAME                     /
    /                                               /
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                     QTYPE                     |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                     QCLASS                    |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
*/
struct dns_question
{
	char *qname;
	uint16_t qtype;
	uint16_t qclass;
};

/*
$ nslookup
> set type=soa
> poftut.com
Server:		10.129.181.107
Address:	10.129.181.107#53

Non-authoritative answer:
poftut.com
	origin = edna.ns.cloudflare.com
	mail addr = dns.cloudflare.com
	serial = 2038439824  // 序列号
	refresh = 10000   // 刷新时间
	retry = 2400  // 重试时间
	expire = 604800  // 过期时间
	minimum = 3600   // TTL
 
Authoritative answers can be found from:

*/

/**
 * @brief soa : start of authority
 * SOA Record: 授权机构记录，记录ns中哪个是主服务器。
 * https://en.wikipedia.org/wiki/SOA_record
 * @mname Primary master name server for this zone
 * @rname Email address of the administrator responsible for this zone. 
 * @serial 代表这个zone file 的版本，每当zone file 内容有变动，name server 管理者就应该增加这个号码，
 * 		   因为slave 会将这个号码与其copy 的那份比对以便决定是否要再copy 一次(即进行zone transfer )。
 * @Refresh slave server 每隔这段时间(秒)，就会检查master server 上的serial number。
 * @retry 当slave server 无法和master 进行serial check时，要每隔几秒retry 一次。
 * @expire 当时间超过Expire 所定的秒数而slave server 都无法和master 取得连络，那么slave 会删除自己的这份copy。
 * @minimum (a.k.a. TTL) 代表这个zone file 中所有record 的内定的TTL 值，也就是其它的DNS server cache 对比record 时，最长不应该超过这个时间。
 */
struct dns_record_soa
{
	char *mname;
	char *rname;
	uint32_t serial;
	int32_t refresh;
	int32_t retry;
	int32_t expire;
	uint32_t minimum;
};

/**
 * @brief SRV record
 * @format: _service._proto.name. ttl IN SRV priority weight port target.
 * https://en.wikipedia.org/wiki/SRV_record
 * @priority 目标主机的优先级，值越小越优先。
 * @weight 相同优先度记录的相对权重，值越大越优先。
 * @port 服务所在的TCP或UDP端口。
 * @target 提供服务的规范主机名，以半角句号结尾。
 */
struct dns_record_srv
{
	uint16_t priority;  // the priority of the target host, lower value means more preferred.
	uint16_t weight;
	uint16_t port;
	char *target;
};

/**
 * @brief 邮件交换记录 (MX record)是域名系统（DNS）中的一种资源记录类型，用于指定负责处理发往收件人域名的邮件服务器。
 * https://en.wikipedia.org/wiki/MX_record
 * @preference  from 0 to 65535 (The priority of the target host, lower value means more preferred.).
 */

/*
Example of an MX record:

example.com	record type:	priority:	value:					sTTL
@				MX				10		mailhost1.example.com	45000
*/
struct dns_record_mx
{
	int16_t preference;  
	char *exchange;  // todo : 这是啥
};

/**
 * @brief 回答部分
 * 
 * name 和上面提问的QNAME一样
 * type 和上面提问的QTYPE一样
 * rclass 和上面的QCLASS一样
 * ttl 生命周期，DNS有缓存机制，这个表示缓存的时间(S)。
 * rdlength 资源长度(即RDATA长度)
 * rdata 资源内容，A为4个字节的地址，AAAA为16个字节的地址，其他的略过
 */
struct dns_record
{
	char *name;
	uint16_t type;
	uint16_t rclass;
	uint32_t ttl;
	uint16_t rdlength;
	void *rdata;
};

typedef struct __dns_parser
{
	void *msgbuf;				// Message with leading length (TCP)
	void *msgbase;				// Message without leading length
	const char *cur;			// Current parser position
	size_t msgsize;
	size_t bufsize;
	char complete;				// Whether parse completed
	char single_packet;			// Response without leading length When UDP
	struct dns_header header;
	struct dns_question question;
	struct list_head answer_list;
	struct list_head authority_list;
	struct list_head additional_list;
} dns_parser_t;

typedef struct __dns_record_cursor
{
	const struct list_head *head;
	const struct list_head *next;
} dns_record_cursor_t;

#ifdef __cplusplus
extern "C"
{
#endif

void dns_parser_init(dns_parser_t *parser);

void dns_parser_set_id(uint16_t id, dns_parser_t *parser);
int dns_parser_set_question(const char *name,
							uint16_t qtype,
							uint16_t qclass,
							dns_parser_t *parser);
int dns_parser_set_question_name(const char *name,
								 dns_parser_t *parser);

int dns_parser_parse_all(dns_parser_t *parser);
int dns_parser_append_message(const void *buf, size_t *n,
							  dns_parser_t *parser);

void dns_parser_deinit(dns_parser_t *parser);

int dns_record_cursor_next(struct dns_record **record,
						   dns_record_cursor_t *cursor);

int dns_record_cursor_find_cname(const char *name,
								 const char **cname,
								 dns_record_cursor_t *cursor);

const char *dns_type2str(int type);
const char *dns_class2str(int dnsclass);
const char *dns_opcode2str(int opcode);
const char *dns_rcode2str(int rcode);

#ifdef __cplusplus
}
#endif

static inline void dns_answer_cursor_init(dns_record_cursor_t *cursor,
										  const dns_parser_t *parser)
{
	cursor->head = &parser->answer_list;
	cursor->next = cursor->head;
}

static inline void dns_authority_cursor_init(dns_record_cursor_t *cursor,
											 const dns_parser_t *parser)
{
	cursor->head = &parser->authority_list;
	cursor->next = cursor->head;
}

static inline void dns_additional_cursor_init(dns_record_cursor_t *cursor,
											  const dns_parser_t *parser)
{
	cursor->head = &parser->additional_list;
	cursor->next = cursor->head;
}

static inline void dns_record_cursor_deinit(dns_record_cursor_t *cursor)
{
}

#endif

