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

#ifndef _POLLER_H_
#define _POLLER_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include <openssl/ssl.h>

typedef struct __poller poller_t;
typedef struct __poller_message poller_message_t;

/*
一些设计解释

1. 按理READ操作的create_message和LISTEN操作的accept是对应关系，
但是create_message是全局的（poller_params里），
而accpet却在poller_data里，非常不对称。
这个原因看我们主分枝的poller.h就明白了，poller_data里第一个union我们放了一个SSL *，
如果吧create_message放到poller_data里，SSL *就不得不独立占有一个域，那么每个poller_data会增加8个字节（64bits系统下）。

2. 单个poller对象是单工单线程的！需要多线程可以用mpoller模块，通用对fd的取模选择一个poller对象。
而同时对一个fd进行读写的话（不多见），可以先调用dup()产生一个复制再加进来。

非SSL版本的poller是可以实现SSL通讯的！但是比较麻烦，需要自己做协议叠加。
*/


// poller_message_t是一条完整的消息，这是一个变长结构体，需要而且只需要实现append。
struct __poller_message
{
	// append的参数const void *buf和size_t *size分别是收到的数据以及数据的长度。
	// 如果buf内容跨越一条消息（所谓tcp粘包），可通过修改*size表示这条消息消费的字节数，而剩下的数据被下一条消息消费。
	// append返回1表示本条消息已经完整（callback会得到一次SUCCESS状态），返回0表示需要继续传输，返回-1表示失败。
	// 失败时必须填写的errno，用于之后的失败返回。
	int (*append)(const void *, size_t *, poller_message_t *);
	char data[0];   // 柔性数组
};


/*
1. 以读操作为例，poller_add时一般需要把message置为NULL（除非是续传，message指向之前的不完整消息），
context置为poller_params里create_message是的参数，
operation置为PD_OP_READ，
fd则是nonblocking的文件fd。

callback以SUCCESS返回时，message指向成功读取的消息，
其它的域与poller_add传入的相同，fd的读操作继续进行。

以非SUCCESS返回时，message可能为NULL，也可能指向一个不完整的消息，fd交还用户（可以重新poller_add续传）

2. 写操作时，write_iov指向要发送的离散内存，iovcnt表示write_iov的个数。
数据发送完毕，callback里以FINISHED状态返回（写是一次性操作）。
对于TCP，建议先同步写，同步写不能完全写入再交给poller。对于UDP，请不要调用poller的写，直接同步写就好了。

*/
struct poller_data
{
#define PD_OP_READ			1
#define PD_OP_WRITE			2
#define PD_OP_LISTEN		3
#define PD_OP_CONNECT		4
#define PD_OP_SSL_READ		PD_OP_READ
#define PD_OP_SSL_WRITE		PD_OP_WRITE
#define PD_OP_SSL_ACCEPT	5
#define PD_OP_SSL_CONNECT	6
#define PD_OP_SSL_SHUTDOWN	7
#define PD_OP_EVENT			8
#define PD_OP_NOTIFY		9
#define PD_OP_TIMER			10
	/*
	operation可以分为两类：
	1. 一次性的操作，包括WRITE，CONNECT和TIMER。
	这几个操作正常结束是以FINISHED状态从callback里返回，还有其他情况以ERROR，DELETED，MODIFIED或STOPPED从callback返回。返回之后fd就归还用户了。
	
	2. 持续性操作。包括READ，LISTEN，EVENT，NOTIFY。
	EVENT和NOTIFY我们先不关心。
	持续性操作最终同样通过FINISHED，ERROR，DELETED，MODIFIED或STOPPED从callback返回并将fd归还用户。
	但是，在归还之前，可能返回多次SUCCESS状态的结果。
	以READ为例，就是返回从fd上切下来的一条完整消息。
	而LISTEN则是accept下来的一个accept_fd（不同于传进来的那个listen_fd）。
	*/
	short operation;
	unsigned short iovcnt;
	int fd;
	union
	{
		SSL *ssl;
		void *(*accept)(const struct sockaddr *, socklen_t, int, void *);
		void *(*event)(void *);
		void *(*notify)(void *, void *);  
	};
	void *context;
	union
	{
		poller_message_t *message;
		struct iovec *write_iov;
		void *result;
	};
};

struct poller_result
{
// 每个传给poller的fd，最终通过后6种状态（PR_ST_XXXX）返回给用户
// 而对于READ或LISTEN操作还可能得到若干次的SUCCESS
// 返回的poller_result的state为PR_ST_ERROR时，error域保存了系统错误码errno。其他状态下，error域为0
// 特别指出，READ操作以FINISHED状态返回时说明fd上读到0字节，意味着socket被对方关闭或管道写端被关闭
// 而LISTEN操作则不会以FINISHED状态返回
#define PR_ST_SUCCESS		0
#define PR_ST_FINISHED		1
#define PR_ST_ERROR			2
#define PR_ST_DELETED		3
#define PR_ST_MODIFIED		4
#define PR_ST_STOPPED		5
	int state;
	int error;
	// poller_result结构里包含一个poller_data（poller_add传入的结构）
	struct poller_data data;
	/* In callback, spaces of six pointers are available from here. */
};

struct poller_params
{
	size_t max_open_files;
	poller_message_t *(*create_message)(void *);
	int (*partial_written)(size_t, void *);
	/*
	poller一切结果都通过callback返回，也就是poller_params里的void (*callback)(struct poller_result *, void *)。
	callback的第二个参数void *是poller_params里的context。

	poller_start时，poller内部会创建一个线程进行epoll操作。
	callback是在这个线程里调起的。
	因此，callback是在同一个线程里串行执行的。
	因为一个读操作会得到很多条SUCCESS消息，保序的callback是必须的。
	传给callback的poller_result *，没有const修饰，这个result指针是需要用户free的。
	这么做的目的是减少malloc和free次数，以及减少不必要的内存拷贝。
	此外，callback里的poller_result结尾处，有6个指针的空间可以利用，相当于你其实得到这个的一个结构：
	struct poller_result_EXT
	{
		int state;
		int error;
		struct poller_data data;
		char avail[6 * sizeof (void *)];  
	};
	一切都是为了减少内存分配释放。总之最重要的是记得要自行free。
	*/
	void (*callback)(struct poller_result *, void *);  
	void *context;
};

#ifdef __cplusplus
extern "C"
{
#endif

/*
接口的线程安全性
除了poller_start和poller_stop这对操作明显需要串行依次调用之外，
其他调用（当然不包括poller_create和poller_destroy）完全线程安全，且可以和poller_start与poller_stop并发的调用。
*/

poller_t *poller_create(const struct poller_params *params);
int poller_start(poller_t *poller);
int poller_add(const struct poller_data *data, int timeout, poller_t *poller);
int poller_del(int fd, poller_t *poller);
int poller_mod(const struct poller_data *data, int timeout, poller_t *poller);
int poller_set_timeout(int fd, int timeout, poller_t *poller);
int poller_add_timer(const struct timespec *value, void *context,
					 poller_t *poller);
void poller_stop(poller_t *poller);
void poller_destroy(poller_t *poller);

#ifdef __cplusplus
}
#endif

#endif

