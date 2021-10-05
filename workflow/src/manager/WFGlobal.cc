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

  Authors: Wu Jiaxu (wujiaxu@sogou-inc.com)
           Liu Kai (liukaidx@sogou-inc.com)
           Xie Han (xiehan@sogou-inc.com)
*/

#include <assert.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <string>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/engine.h>
#include <openssl/conf.h>
#include <openssl/crypto.h>
#include "WFGlobal.h"
#include "EndpointParams.h"
#include "CommScheduler.h"
#include "DnsCache.h"
#include "RouteManager.h"
#include "Executor.h"
#include "WFTask.h"
#include "WFTaskError.h"
#include "WFNameService.h"
#include "WFDnsResolver.h"
#include "WFDnsClient.h"
#include "logger.h"

class __WFGlobal
{
public:
	static __WFGlobal *get_instance()
	{
		static __WFGlobal kInstance;
		return &kInstance;
	}

	const WFGlobalSettings *get_global_settings() const
	{
		return &settings_;
	}

	void set_global_settings(const WFGlobalSettings *settings)
	{
		settings_ = *settings;
	}

	const char *get_default_port(const std::string& scheme)
	{
		const auto it = static_scheme_port_.find(scheme);

		if (it != static_scheme_port_.end())
			return it->second;

		const char *port = NULL;
		user_scheme_port_mutex_.lock();
		const auto it2 = user_scheme_port_.find(scheme);

		if (it2 != user_scheme_port_.end())
			port = it2->second.c_str();

		user_scheme_port_mutex_.unlock();
		return port;
	}

	void register_scheme_port(const std::string& scheme, unsigned short port)
	{
		user_scheme_port_mutex_.lock();
		user_scheme_port_[scheme] = std::to_string(port);
		user_scheme_port_mutex_.unlock();
	}

	void sync_operation_begin()
	{
		bool inc;

		sync_mutex_.lock();
		inc = ++sync_count_ > sync_max_;

		if (inc)
			sync_max_ = sync_count_;
		sync_mutex_.unlock();
		if (inc)
			WFGlobal::get_scheduler()->increase_handler_thread();
	}

	void sync_operation_end()
	{
		sync_mutex_.lock();
		sync_count_--;
		sync_mutex_.unlock();
	}

private:
	__WFGlobal();

private:
	struct WFGlobalSettings settings_;
	std::unordered_map<std::string, const char *> static_scheme_port_;
	std::unordered_map<std::string, std::string> user_scheme_port_;
	std::mutex user_scheme_port_mutex_;
	std::mutex sync_mutex_;
	int sync_count_;
	int sync_max_;
};

__WFGlobal::__WFGlobal() : settings_(GLOBAL_SETTINGS_DEFAULT)
{
	static_scheme_port_["dns"] = "53";
	static_scheme_port_["Dns"] = "53";
	static_scheme_port_["DNS"] = "53";

	static_scheme_port_["dnss"] = "853";
	static_scheme_port_["Dnss"] = "853";
	static_scheme_port_["DNSs"] = "853";
	static_scheme_port_["DNSS"] = "853";

	static_scheme_port_["http"] = "80";
	static_scheme_port_["Http"] = "80";
	static_scheme_port_["HTTP"] = "80";

	static_scheme_port_["https"] = "443";
	static_scheme_port_["Https"] = "443";
	static_scheme_port_["HTTPs"] = "443";
	static_scheme_port_["HTTPS"] = "443";

	static_scheme_port_["redis"] = "6379";
	static_scheme_port_["Redis"] = "6379";
	static_scheme_port_["REDIS"] = "6379";

	static_scheme_port_["rediss"] = "6379";
	static_scheme_port_["Rediss"] = "6379";
	static_scheme_port_["REDISs"] = "6379";
	static_scheme_port_["REDISS"] = "6379";

	static_scheme_port_["mysql"] = "3306";
	static_scheme_port_["Mysql"] = "3306";
	static_scheme_port_["MySql"] = "3306";
	static_scheme_port_["MySQL"] = "3306";
	static_scheme_port_["MYSQL"] = "3306";

	static_scheme_port_["mysqls"] = "3306";
	static_scheme_port_["Mysqls"] = "3306";
	static_scheme_port_["MySqls"] = "3306";
	static_scheme_port_["MySQLs"] = "3306";
	static_scheme_port_["MYSQLs"] = "3306";
	static_scheme_port_["MYSQLS"] = "3306";

	static_scheme_port_["kafka"] = "9092";
	static_scheme_port_["Kafka"] = "9092";
	static_scheme_port_["KAFKA"] = "9092";

	sync_count_ = 0;
	sync_max_ = 0;
}

#if OPENSSL_VERSION_NUMBER < 0x10100000L
static std::mutex *__ssl_mutex;

static void ssl_locking_callback(int mode, int type, const char* file, int line)
{
	if (mode & CRYPTO_LOCK)
		__ssl_mutex[type].lock();
	else if (mode & CRYPTO_UNLOCK)
		__ssl_mutex[type].unlock();
}
#endif

class __SSLManager
{
public:
	static __SSLManager *get_instance()
	{
		static __SSLManager kInstance;
		return &kInstance;
	}

	SSL_CTX *get_ssl_client_ctx() { return ssl_client_ctx_; }
	SSL_CTX *new_ssl_server_ctx() { return SSL_CTX_new(SSLv23_server_method()); }

private:
	__SSLManager()
	{
#if OPENSSL_VERSION_NUMBER < 0x10100000L
		__ssl_mutex = new std::mutex[CRYPTO_num_locks()];
		CRYPTO_set_locking_callback(ssl_locking_callback);
		SSL_library_init();
		SSL_load_error_strings();
		//ERR_load_crypto_strings();
		//OpenSSL_add_all_algorithms();
#endif

		ssl_client_ctx_ = SSL_CTX_new(SSLv23_client_method());
		assert(ssl_client_ctx_ != NULL);
	}

	~__SSLManager()
	{
		SSL_CTX_free(ssl_client_ctx_);

#if OPENSSL_VERSION_NUMBER < 0x10100000L
		//free ssl to avoid memory leak
		FIPS_mode_set(0);
		CRYPTO_set_locking_callback(NULL);
# ifdef CRYPTO_LOCK_ECDH
		CRYPTO_THREADID_set_callback(NULL);
# else
		CRYPTO_set_id_callback(NULL);
# endif
		ENGINE_cleanup();
		CONF_modules_unload(1);
		ERR_free_strings();
		EVP_cleanup();
# ifdef CRYPTO_LOCK_ECDH
		ERR_remove_thread_state(NULL);
# else
		ERR_remove_state(0);
# endif
		CRYPTO_cleanup_all_ex_data();
		sk_SSL_COMP_free(SSL_COMP_get_compression_methods());
		delete []__ssl_mutex;
#endif
	}

private:
	SSL_CTX *ssl_client_ctx_;
};

class IOServer : public IOService
{
public:
	IOServer(CommScheduler *scheduler):
		scheduler_(scheduler),
		flag_(true)
	{}

	int bind()
	{
		mutex_.lock();
		flag_ = false;

		int ret = scheduler_->io_bind(this);

		if (ret < 0)
			flag_ = true;

		mutex_.unlock();
		return ret;
	}

	void deinit()
	{
		std::unique_lock<std::mutex> lock(mutex_);
		while (!flag_)
			cond_.wait(lock);

		lock.unlock();
		IOService::deinit();
	}

private:
	virtual void handle_unbound()
	{
		mutex_.lock();
		flag_ = true;
		cond_.notify_one();
		mutex_.unlock();
	}

	virtual void handle_stop(int error)
	{
		scheduler_->io_unbind(this);
	}

	CommScheduler *scheduler_;
	std::mutex mutex_;
	std::condition_variable cond_;
	bool flag_;
};

class __DnsManager
{
public:
	ExecQueue *get_dns_queue() { return &dns_queue_; }
	Executor *get_dns_executor() { return &dns_executor_; }

	__DnsManager()
	{
		int ret;

		ret = dns_queue_.init();
		if (ret < 0)
			abort();

		ret = dns_executor_.init(__WFGlobal::get_instance()->
											 get_global_settings()->
											 dns_threads);
		if (ret < 0)
			abort();
	}

	~__DnsManager()
	{
		dns_executor_.deinit();
		dns_queue_.deinit();
	}

private:
	ExecQueue dns_queue_;
	Executor dns_executor_;
};

class __CommManager
{
public:
	static __CommManager *get_instance()
	{
		static __CommManager kInstance;
		return &kInstance;
	}

	CommScheduler *get_scheduler() { return &scheduler_; }
	RouteManager *get_route_manager() { return &route_manager_; }
	IOService *get_io_service()
	{
		if (!io_flag_)
		{
			io_mutex_.lock();
			if (!io_flag_)
			{
				io_server_ = new IOServer(&scheduler_);
				//todo EAGAIN 65536->2
				if (io_server_->init(8192) < 0)
					abort();

				if (io_server_->bind() < 0)
					abort();

				io_flag_ = true;
			}

			io_mutex_.unlock();
		}

		return io_server_;
	}

	ExecQueue *get_dns_queue()
	{
		return get_dns_manager_safe()->get_dns_queue();
	}

	Executor *get_dns_executor()
	{
		return get_dns_manager_safe()->get_dns_executor();
	}

private:
	__CommManager():
		io_server_(NULL),
		io_flag_(false),
		dns_manager_(NULL),
		dns_flag_(false)
	{
		LOG_TRACE("__CommManager creator");
		const auto *settings = __WFGlobal::get_instance()->get_global_settings();
		if (scheduler_.init(settings->poller_threads,
							settings->handler_threads) < 0)
			abort();

		signal(SIGPIPE, SIG_IGN);
	}

	~__CommManager()
	{
		if (dns_manager_)
			delete dns_manager_;

		scheduler_.deinit();
		if (io_server_)
		{
			io_server_->deinit();
			delete io_server_;
		}
	}

	__DnsManager *get_dns_manager_safe()
	{
		if (!dns_flag_)
		{
			dns_mutex_.lock();
			if (!dns_flag_)
			{
				dns_manager_ = new __DnsManager();
				dns_flag_ = true;
			}

			dns_mutex_.unlock();
		}

		return dns_manager_;
	}

private:
	CommScheduler scheduler_;
	RouteManager route_manager_;
	IOServer *io_server_;
	volatile bool io_flag_;
	std::mutex io_mutex_;
	__DnsManager *dns_manager_;
	volatile bool dns_flag_;
	std::mutex dns_mutex_;
};

class __DnsCache
{
public:
	static __DnsCache *get_instance()
	{
		static __DnsCache kInstance;
		return &kInstance;
	}

	DnsCache *get_dns_cache() { return &dns_cache_; }

private:
	__DnsCache() { }

	~__DnsCache() { }

private:
	DnsCache dns_cache_;
};

class __ExecManager
{
protected:
	using ExecQueueMap = std::unordered_map<std::string, ExecQueue *>;

public:
	static __ExecManager *get_instance()
	{
		static __ExecManager kInstance;
		return &kInstance;
	}
	/**
	 * @brief 创建ExecQueue
	 * 
	 * @param queue_name 
	 * @return ExecQueue* 
	 */
	ExecQueue *get_exec_queue(const std::string& queue_name)
	{
		ExecQueue *queue = NULL;
		/*
		关于计算队列名
		我们的计算任务并没有优化级的概念，唯一可以影响调度顺序的是计算任务的队列名，本示例中队列名为字符串"add"。
		队列名的指定非常简单，需要说明以下几点：

		1. 队列名是一个静态字符串，不可以无限产生新的队列名。例如不可以根据请求id来产生队列名，因为内部会为每个队列分配一小块资源。
		当计算线程没有被100%占满，所有任务都是实时调起，队列名没有任何影响。

		2. 如果一个服务流程里有多个计算步骤，穿插在多个网络通信之间，可以简单的给每种计算步骤起一个名字，这个会比整体用一个名字要好。
		
		3.如果所有计算任务用同一个名字，那么所有任务的被调度的顺序与提交顺序一致，在某些场景下会影响平均响应时间。
		
		4. 每种计算任务有一个独立名字，那么相当于每种任务之间是公平调度的，而同一种任务内部是顺序调度的，实践效果更好。
		
		总之，除非机器的计算负载已经非常繁重，否则没有必要特别关心队列名，只要每种任务起一个名字就可以了。
		*/
		pthread_rwlock_rdlock(&rwlock_);

		const auto iter = queue_map_.find(queue_name);
		// 先查找队列名
		if (iter != queue_map_.cend())
			queue = iter->second;

		pthread_rwlock_unlock(&rwlock_);

		// 如果还没有此队列名，那么new一个, 并初始化
		if (!queue)
		{
			queue = new ExecQueue();
			if (queue->init() < 0)
			{
				delete queue;
				queue = NULL;
			}
			else
			{
				pthread_rwlock_wrlock(&rwlock_);
				// 存起来
				const auto ret = queue_map_.emplace(queue_name, queue);

				if (!ret.second)
				{
					queue->deinit();
					delete queue;
					queue = ret.first->second;
				}

				pthread_rwlock_unlock(&rwlock_);
			}
		}

		return queue;
	}

	Executor *get_compute_executor() { return &compute_executor_; }

private:
	__ExecManager():
		rwlock_(PTHREAD_RWLOCK_INITIALIZER)
	{
		LOG_TRACE("__ExecManager creator");
		int compute_threads = __WFGlobal::get_instance()->
										  get_global_settings()->
										  compute_threads;

		if (compute_threads <= 0)
			compute_threads = sysconf(_SC_NPROCESSORS_ONLN);

		if (compute_executor_.init(compute_threads) < 0)
			abort();
	}

	~__ExecManager()
	{
		compute_executor_.deinit();

		for (auto& kv : queue_map_)
		{
			kv.second->deinit();
			delete kv.second;
		}
	}

private:
	pthread_rwlock_t rwlock_;
	ExecQueueMap queue_map_;
	Executor compute_executor_;
};

class __NameServiceManager
{
public:
	static __NameServiceManager *get_instance()
	{
		static __NameServiceManager kInstance;
		return &kInstance;
	}

public:
	WFDnsResolver *get_dns_resolver() { return &resolver_; }
	WFNameService *get_name_service() { return &service_; }

private:
	WFDnsResolver resolver_;
	WFNameService service_;

public:
	__NameServiceManager() : service_(&resolver_) { }
};

#define MAX(x, y)	((x) >= (y) ? (x) : (y))
#define HOSTS_LINEBUF_INIT_SIZE	128

static void __split_merge_str(const char *p, bool is_nameserver,
							  std::string& result)
{
	// 格式 : 
	/*
	nameserver 10.0.12.210
	search foobar.com foo.com
	options ndots:5
	*/

	const char *start;
	
	if (!isspace(*p)) // 如果从开始不是空格，则格式不正确，直接返回
		return;

	while (1)
	{
		while (isspace(*p))
			p++;
		// 从第一个不为空格的地方开始
		start = p; // 此处start记录下p的位置，然后去移动p
		// ; 或者 # 开头的为注释
		// 如果p不在 '\0' 结束处 且 不是 # 且 不是 ; 且不为空格，则往后移动
		while (*p && *p != '#' && *p != ';' && !isspace(*p))
			p++;

		if (start == p)  // 如果都没动，这一排后面是注释或者后面没有东西，就直接break了
			break;
		// 第一次传入的url是一个空的str，拿进来拼凑出url
		// 第一个之后都用 ',' 分割开
		if (!result.empty())
			result.push_back(',');
		
		std::string str(start, p); 
		if (is_nameserver)
		{
			struct in6_addr buf;
			// 如果是nameserver字段的话，那么这里读出的str则是 ip 地址
			// 这里如果是ipv6则[str]格式
			if (inet_pton(AF_INET6, str.c_str(), &buf) > 0)
				str = "[" + str + "]";
		}

		result.append(str);
	}
}

static inline const char *__try_options(const char *p, const char *q,
										const char *r)
{
	size_t len = strlen(r);  // 要查找的字段长度
	// 首先在一个字符串中找这个字段，要大于他才行， 然后看这么长是否有是这个字段
	// 如果是则返回末尾的这个位置，否则NULL
	if ((size_t)(q - p) >= len && strncmp(p, r, len) == 0)
		return p + len;
	return NULL;
}

static void __set_options(const char *p,
						  int *ndots, int *attempts, bool *rotate)
{
	// 和__split_merge_str 逻辑一样的找字段
	const char *start;
	const char *opt;

	if (!isspace(*p))
		return;

	while (1)
	{
		while (isspace(*p))
			p++;

		start = p;
		while (*p && *p != '#' && *p != ';' && !isspace(*p))
			p++;

		if (start == p)
			break;
		// 主要找以下几个字段
		if ((opt = __try_options(start, p, "ndots:")) != NULL)
			*ndots = atoi(opt);
		else if ((opt = __try_options(start, p, "attempts:")) != NULL)
			*attempts = atoi(opt);
		else if ((opt = __try_options(start, p, "rotate")) != NULL)
			*rotate = true;  // rotate 出现了就是true，没有其他值
			// 格式 : options rotate
	}
}

/**
 * @brief 这个函数，传入path(resolve.conf), 然后读出里面的url， search_list，ndots， attempts，rotate出来
 * 
 * @param [in] path 
 * @param [out] url 
 * @param [out] search_list 
 * @param [out] ndots 
 * @param [out] attempts 
 * @param [out] rotate 
 * @return int 0 成功，-1 失败
 */
static int __parse_resolv_conf(const char *path,
							   std::string& url, std::string& search_list,
							   int *ndots, int *attempts, bool *rotate)
{
	size_t bufsize = 0;
	char *line = NULL;
	FILE *fp;
	int ret;
	// 打开resolve.conf文件
	fp = fopen(path, "r");
	if (!fp)
		return -1;
	// 然后一排一排的读取出来
	while ((ret = getline(&line, &bufsize, fp)) > 0)
	{
		// 然后判断是哪些字段
		// nameserver x.x.x.x该选项用来制定DNS服务器的，可以配置多个nameserver指定多个DNS。
		if (strncmp(line, "nameserver", 10) == 0)
			__split_merge_str(line + 10, true, url);
		else if (strncmp(line, "search", 6) == 0)
			// search 111.com 222.com该选项可以用来指定多个域名，中间用空格或tab键隔开。
			// 原来当访问的域名不能被DNS解析时，resolver会将该域名加上search指定的参数，重新请求DNS，
			// 直到被正确解析或试完search指定的列表为止。
			__split_merge_str(line + 6, false, search_list);
		else if (strncmp(line, "options", 7) == 0)
			// 接下来就是这只options选项了
			__set_options(line + 7, ndots, attempts, rotate);
	}

	ret = ferror(fp) ? -1 : 0;
	free(line);
	fclose(fp);
	return ret;
}

class __DnsClientManager
{
public:
	static __DnsClientManager *get_instance()
	{
		static __DnsClientManager kInstance;
		return &kInstance;
	}

public:
	WFDnsClient *get_dns_client() { return client; }

private:
	__DnsClientManager()
	{
		// default "/etc/resolv.conf"
		const char *path = WFGlobal::get_global_settings()->resolv_conf_path;

		client = NULL;
		// 如果有resolv.conf路径
		if (path && path[0])
		{
			// man 5 resolv.conf
			int ndots = 1;  // 设置调用res_query()解析域名时域名至少包含的点的数量
			int attempts = 2;  // 设置resolver向DNS服务器发起域名解析的请求次数。默认值RES_DFLRETRY=2，参见<resolv.h>
			bool rotate = false;  // 在_res.options中设置RES_ROTATE，采用轮询方式访问nameserver，实现负载均衡
			std::string url; 
			// 来当访问的域名不能被DNS解析时，resolver会将该域名加上search指定的参数，重新请求DNS
			// 直到被正确解析或试完search指定的列表为止。
			std::string search;
			
			__parse_resolv_conf(path, url, search, &ndots, &attempts, &rotate);
			if (url.size() == 0)  // 如果没有的话，默认为google的 8.8.8.8
				url = "8.8.8.8";

			client = new WFDnsClient;  

			if (client->init(url, search, ndots, attempts, rotate) >= 0)
				return;  // 成功
			
			// 失败则把client复原
			delete client;
			client = NULL;
		}
	}

	~__DnsClientManager()
	{
		if (client)
		{
			client->deinit();
			delete client;
		}
	}

	WFDnsClient *client;
};

WFDnsClient *WFGlobal::get_dns_client()
{
	return __DnsClientManager::get_instance()->get_dns_client();
}

CommScheduler *WFGlobal::get_scheduler()
{
	return __CommManager::get_instance()->get_scheduler();
}

DnsCache *WFGlobal::get_dns_cache()
{
	return __DnsCache::get_instance()->get_dns_cache();
}

RouteManager *WFGlobal::get_route_manager()
{
	return __CommManager::get_instance()->get_route_manager();
}

SSL_CTX *WFGlobal::get_ssl_client_ctx()
{
	return __SSLManager::get_instance()->get_ssl_client_ctx();
}

SSL_CTX *WFGlobal::new_ssl_server_ctx()
{
	return __SSLManager::get_instance()->new_ssl_server_ctx();
}

ExecQueue *WFGlobal::get_exec_queue(const std::string& queue_name)
{
	return __ExecManager::get_instance()->get_exec_queue(queue_name);
}

Executor *WFGlobal::get_compute_executor()
{
	return __ExecManager::get_instance()->get_compute_executor();
}

IOService *WFGlobal::get_io_service()
{
	return __CommManager::get_instance()->get_io_service();
}

ExecQueue *WFGlobal::get_dns_queue()
{
	return __CommManager::get_instance()->get_dns_queue();
}

Executor *WFGlobal::get_dns_executor()
{
	return __CommManager::get_instance()->get_dns_executor();
}

WFNameService *WFGlobal::get_name_service()
{
	return __NameServiceManager::get_instance()->get_name_service();
}

WFDnsResolver *WFGlobal::get_dns_resolver()
{
	return __NameServiceManager::get_instance()->get_dns_resolver();
}

const char *WFGlobal::get_default_port(const std::string& scheme)
{
	return __WFGlobal::get_instance()->get_default_port(scheme);
}

void WFGlobal::register_scheme_port(const std::string& scheme,
									unsigned short port)
{
	__WFGlobal::get_instance()->register_scheme_port(scheme, port);
}

const WFGlobalSettings *WFGlobal::get_global_settings()
{
	return __WFGlobal::get_instance()->get_global_settings();
}

void WORKFLOW_library_init(const WFGlobalSettings *settings)
{
	__WFGlobal::get_instance()->set_global_settings(settings);
}

void WFGlobal::sync_operation_begin()
{
	__WFGlobal::get_instance()->sync_operation_begin();
}

void WFGlobal::sync_operation_end()
{
	__WFGlobal::get_instance()->sync_operation_end();
}

static inline const char *__get_ssl_error_string(int error)
{
	switch (error)
	{
	case SSL_ERROR_NONE:
		return "SSL Error None";

	case SSL_ERROR_ZERO_RETURN:
		return "SSL Error Zero Return";

	case SSL_ERROR_WANT_READ:
		return "SSL Error Want Read";

	case SSL_ERROR_WANT_WRITE:
		return "SSL Error Want Write";

	case SSL_ERROR_WANT_CONNECT:
		return "SSL Error Want Connect";

	case SSL_ERROR_WANT_ACCEPT:
		return "SSL Error Want Accept";

	case SSL_ERROR_WANT_X509_LOOKUP:
		return "SSL Error Want X509 Lookup";

#ifdef SSL_ERROR_WANT_ASYNC
	case SSL_ERROR_WANT_ASYNC:
		return "SSL Error Want Async";
#endif

#ifdef SSL_ERROR_WANT_ASYNC_JOB
	case SSL_ERROR_WANT_ASYNC_JOB:
		return "SSL Error Want Async Job";
#endif

#ifdef SSL_ERROR_WANT_CLIENT_HELLO_CB
	case SSL_ERROR_WANT_CLIENT_HELLO_CB:
		return "SSL Error Want Client Hello CB";
#endif

	case SSL_ERROR_SYSCALL:
		return "SSL System Error";

	case SSL_ERROR_SSL:
		return "SSL Error SSL";

	default:
		break;
	}

	return "Unknown";
}

static inline const char *__get_task_error_string(int error)
{
	switch (error)
	{
	case WFT_ERR_URI_PARSE_FAILED:
		return "URI Parse Failed";

	case WFT_ERR_URI_SCHEME_INVALID:
		return "URI Scheme Invalid";

	case WFT_ERR_URI_PORT_INVALID:
		return "URI Port Invalid";

	case WFT_ERR_UPSTREAM_UNAVAILABLE:
		return "Upstream Unavailable";

	case WFT_ERR_HTTP_BAD_REDIRECT_HEADER:
		return "Http Bad Redirect Header";

	case WFT_ERR_HTTP_PROXY_CONNECT_FAILED:
		return "Http Proxy Connect Failed";

	case WFT_ERR_REDIS_ACCESS_DENIED:
		return "Redis Access Denied";

	case WFT_ERR_REDIS_COMMAND_DISALLOWED:
		return "Redis Command Disallowed";

	case WFT_ERR_MYSQL_HOST_NOT_ALLOWED:
		return "MySQL Host Not Allowed";

	case WFT_ERR_MYSQL_ACCESS_DENIED:
		return "MySQL Access Denied";

	case WFT_ERR_MYSQL_INVALID_CHARACTER_SET:
		return "MySQL Invalid Character Set";

	case WFT_ERR_MYSQL_COMMAND_DISALLOWED:
		return "MySQL Command Disallowed";

	case WFT_ERR_MYSQL_QUERY_NOT_SET:
		return "MySQL Query Not Set";

	case WFT_ERR_MYSQL_SSL_NOT_SUPPORTED:
		return "MySQL SSL Not Supported";

	case WFT_ERR_KAFKA_PARSE_RESPONSE_FAILED:
		return "Kafka parse response failed";

	case WFT_ERR_KAFKA_PRODUCE_FAILED:
		return "Kafka produce api failed";

	case WFT_ERR_KAFKA_FETCH_FAILED:
		return "Kafka fetch api failed";

	case WFT_ERR_KAFKA_CGROUP_FAILED:
		return "Kafka cgroup failed";

	case WFT_ERR_KAFKA_COMMIT_FAILED:
		return "Kafka commit api failed";

	case WFT_ERR_KAFKA_META_FAILED:
		return "Kafka meta api failed";

	case WFT_ERR_KAFKA_LEAVEGROUP_FAILED:
		return "Kafka leavegroup failed";

	case WFT_ERR_KAFKA_API_UNKNOWN:
		return "Kafka api type unknown";

	case WFT_ERR_KAFKA_VERSION_DISALLOWED:
		return "Kafka broker version not supported";

	default:
		break;
	}

	return "Unknown";
}

const char *WFGlobal::get_error_string(int state, int error)
{
	switch (state)
	{
	case WFT_STATE_SUCCESS:
		return "Success";

	case WFT_STATE_TOREPLY:
		return "To Reply";

	case WFT_STATE_NOREPLY:
		return "No Reply";

	case WFT_STATE_SYS_ERROR:
		return strerror(error);

	case WFT_STATE_SSL_ERROR:
		return __get_ssl_error_string(error);

	case WFT_STATE_DNS_ERROR:
		return gai_strerror(error);

	case WFT_STATE_TASK_ERROR:
		return __get_task_error_string(error);

	case WFT_STATE_UNDEFINED:
		return "Undefined";

	default:
		break;
	}

	return "Unknown";
}

