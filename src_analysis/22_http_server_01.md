# workflow 源码解析 12 : http server

项目源码 : https://github.com/sogou/workflow

更加详细的源码注释可看 : https://github.com/chanchann/workflow_annotation

我们先看最简单的http例子

https://github.com/chanchann/workflow_annotation/blob/main/demos/07_http/http_no_reply.cc

## 流程分析

## 启动 WFServerBase::start

当我们构造除了一个server，然后start

```cpp
int WFServerBase::start(int family, const char *host, unsigned short port,
						const char *cert_file, const char *key_file)
{
	struct addrinfo hints = {
		.ai_flags		=	AI_PASSIVE,
		.ai_family		=	family,
		.ai_socktype	=	SOCK_STREAM,
	};
	struct addrinfo *addrinfo;
	char port_str[PORT_STR_MAX + 1];
	int ret;
    ...
	snprintf(port_str, PORT_STR_MAX + 1, "%d", port);
	getaddrinfo(host, port_str, &hints, &addrinfo);
    start(addrinfo->ai_addr, (socklen_t)addrinfo->ai_addrlen,
                cert_file, key_file);
    freeaddrinfo(addrinfo);
    ...
}
```

```
// https://man7.org/linux/man-pages/man3/getaddrinfo.3.html

当host = NULL 传进去时

If the AI_PASSIVE flag is specified in hints.ai_flags, and node
is NULL, then the returned socket addresses will be suitable for
bind(2)ing a socket that will accept(2) connections. 

The returned socket address will contain the "wildcard address"
(INADDR_ANY for IPv4 addresses, IN6ADDR_ANY_INIT for IPv6
address).  

The wildcard address is used by applications
(typically servers) that intend to accept connections on any of
the host's network addresses.  

If node is not NULL, then the AI_PASSIVE flag is ignored.
```

这正start的是

```cpp

int WFServerBase::start(const struct sockaddr *bind_addr, socklen_t addrlen,
						const char *cert_file, const char *key_file)
{
    ...
	this->init(bind_addr, addrlen, cert_file, key_file);
    this->scheduler->bind(this);
    ...
}
```

### 