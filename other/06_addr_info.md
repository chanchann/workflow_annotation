# struct addrinfo

```cpp
/* Structure to contain information about address of a service provider.  */
struct addrinfo
{
  int ai_flags;			/* Input flags.  */
  int ai_family;		/* Protocol family for socket.  */
  int ai_socktype;		/* Socket type.  */
  int ai_protocol;		/* Protocol for socket.  */
  socklen_t ai_addrlen;		/* Length of socket address.  */
  struct sockaddr *ai_addr;	/* Socket address for socket.  */
  char *ai_canonname;		/* Canonical name for service location.  */
  struct addrinfo *ai_next;	/* Pointer to next in list.  */
};
```

## getaddrinfo()

IPv4中使用 gethostbyname()函数 完成主机名到地址解析，这个函数仅仅支持IPv4，且不允许调用者指定所需地址类型的任何信息，返回的结构只包含了用于存储IPv4地址的空间

IPv6中引入了getaddrinfo()的新API，它是协议无关的，既可用于IPv4也可用于IPv6。

getaddrinfo()函数能够处理*名字到地址*以及*服务到端口*这两种转换，返回的是一个addrinfo的结构（列表）指针而不是一个地址清单。

这些addrinfo结构随后可由套接口函数直接使用。

如此以来，getaddrinfo函数把协议相关性安全隐藏在这个库函数内部。应用程序只要处理由getaddrinfo函数填写的套接口地址结构。

1. What can it do:

Given node and service, which identify an Internet host and a
service, getaddrinfo() returns one or more addrinfo structures,
each of which contains an Internet address that can be specified
in a call to bind(2) or connect(2). 

2. Combine function

The getaddrinfo() function
combines the functionality provided by the gethostbyname(3) and
getservbyname(3) functions into a single interface, but unlike
the latter functions, getaddrinfo() is reentrant and allows
programs to eliminate IPv4-versus-IPv6 dependencies.

3. Function signature

```cpp
int getaddrinfo(const char *restrict node,
                const char *restrict service,
                const struct addrinfo *restrict hints,
                struct addrinfo **restrict res);
```

## ref

https://man7.org/linux/man-pages/man3/getaddrinfo.3.htmlhttps://man7.org/linux/man-pages/man3/getaddrinfo.3.html

https://www.cxyzjd.com/article/qq_26617115/53585404

https://stackoverflow.com/questions/23401147/what-is-the-difference-between-struct-addrinfo-and-struct-sockaddr