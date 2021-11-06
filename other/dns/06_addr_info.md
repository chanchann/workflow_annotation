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

node: a host name or address string (IPv4 dotted decimal string or IPv6 hexadecimal string)

service: The service name can be a decimal port number or a defined service name, such as ftp, http, etc.

hints: hints is a template, it only uses ai_family, ai_flags, and ai_socktype members to filter addresses. The remaining integer members must be set to 0.

result: This function returns a pointer to the linked list of addrinfo structure through the result pointer parameter.

## analysis

```
open("/etc/nsswitch.conf", O_RDONLY|O_CLOEXEC) = 3
open("/etc/services", O_RDONLY|O_CLOEXEC) = 3
open("/etc/host.conf", O_RDONLY|O_CLOEXEC) = 3
open("/etc/resolv.conf", O_RDONLY|O_CLOEXEC) = 3
open("/etc/hosts", O_RDONLY|O_CLOEXEC) = 3
```

## /etc/nsswitch.conf
 
```
ysy@VM-0-15-ubuntu:~/workflow_annotation/src_analysis$ cat /etc/nsswitch.conf
# /etc/nsswitch.conf
#
# Example configuration of GNU Name Service Switch functionality.
# If you have the `glibc-doc-reference' and `info' packages installed, try:
# `info libc "Name Service Switch"' for information about this file.

passwd:         compat systemd
group:          compat systemd
shadow:         compat
gshadow:        files

hosts:          files dns
networks:       files

protocols:      db files
services:       db files
ethers:         db files
rpc:            db files

netgroup:       nis
```

https://blog.csdn.net/water_cow/article/details/7190880

## /etc/services

This file records the names of network services and their corresponding port numbers and protocols.

```
ysy@VM-0-15-ubuntu:~/workflow_annotation/src_analysis$ cat /etc/services
# Network services, Internet style
#
# Note that it is presently the policy of IANA to assign a single well-known
# port number for both TCP and UDP; hence, officially ports have two entries
# even if the protocol doesn't support UDP operations.
#
# Updated from http://www.iana.org/assignments/port-numbers and other
# sources like http://www.freebsd.org/cgi/cvsweb.cgi/src/etc/services .
# New ports will be added on request if they have been officially assigned
# by IANA and used in the real-world or are needed by a debian package.
# If you need a huge list of used numbers please install the nmap package.

tcpmux          1/tcp                           # TCP port service multiplexer
echo            7/tcp
echo            7/udp
discard         9/tcp           sink null
discard         9/udp           sink null
systat          11/tcp          users
daytime         13/tcp
daytime         13/udp
...
```

##  /etc/host.conf

This file specifies how to resolve the host name

```
ysy@VM-0-15-ubuntu:~/workflow_annotation/src_analysis$ cat /etc/host.conf
# The "order" line is only used by old versions of the C library.
order hosts,bind
multi on
```

"Order hosts, bind" specifies the host name query order, here it is stipulated to query the "/etc/hosts" file first, and then use DNS to resolve the domain name (the opposite is also possible).

"multi on" specifies whether the host specified in the "/etc/hosts" file can have multiple addresses. A host with multiple IP addresses is generally called a multi-home host.

## /etc/resolv.conf

File function: DNS client configuration file, set the IP address and DNS domain name of the DNS server

```
ysy@VM-0-15-ubuntu:~/workflow_annotation/src_analysis$ cat /etc/resolv.conf
# This file is managed by man:systemd-resolved(8). Do not edit.
#
# This is a dynamic resolv.conf file for connecting local clients to the
# internal DNS stub resolver of systemd-resolved. This file lists all
# configured search domains.
#
# Run "systemd-resolve --status" to see details about the uplink DNS servers
# currently in use.
#
# Third party programs must not access this file directly, but only through the
# symlink at /etc/resolv.conf. To manage man:resolv.conf(5) in a different way,
# replace this symlink by a static file or a different symlink.
#
# See man:systemd-resolved.service(8) for details about the supported modes of
# operation for /etc/resolv.conf.

nameserver 127.0.0.53
options edns0
```

## /etc/hosts

```
ysy@VM-0-15-ubuntu:~/workflow_annotation/src_analysis$ cat /etc/hosts
#
127.0.0.1 localhost.localdomain VM-0-15-ubuntu
127.0.0.1 localhost

::1 ip6-localhost ip6-loopback
fe00::0 ip6-localnet
ff00::0 ip6-mcastprefix
ff02::1 ip6-allnodes
ff02::2 ip6-allrouters
ff02::3 ip6-allhosts
```

## priceiple(not official)

1. Read /etc/nsswitch.conf, if you read the hosts file first, search in the hosts file according to the name, if found, return, if not found, use DNS Bind client for domain name resolution processing

2. Read /etc/services, query the correspondence between the general service name and the port number.

3. Read /etc/host.conf This configuration file is the domain name resolution sequence configuration file, and set the resolution sequence method

4. Read the /etc/resolv.conf configuration file, which is used to specify the DNS server to be resolved, as well as the relevant parameters of DNS Bind resolution, such as the number of retries, timeout period, etc.

5. Read the /etc/hosts file, look up the host name and query the static table

6. When the host name is not found in the hosts file, when the DNS Bind client is used for domain name resolution, the /etc/resolv.conf configuration file will be used for domain name resolution, and the resolution method is resolved by resolve. conf file decision

## ref

https://man7.org/linux/man-pages/man3/getaddrinfo.3.htmlhttps://man7.org/linux/man-pages/man3/getaddrinfo.3.html

https://www.cxyzjd.com/article/qq_26617115/53585404

https://stackoverflow.com/questions/23401147/what-is-the-difference-between-struct-addrinfo-and-struct-sockaddr

https://www.programmerall.com/article/4339828807/