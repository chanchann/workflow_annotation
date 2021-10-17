# workflow源码解析14 : DnsRoutine

## DnsInput

```cpp
class DnsInput
{
public:
    ...
	void reset(const std::string& host, unsigned short port)
	{
		host_.assign(host);
		port_ = port;
	}
protected:
	std::string host_;
	unsigned short port_;
    ...
};
```

主要就是存我们需要dns解析的host和port

## DnsOutput

```cpp

class DnsOutput
{
public:
    ...

protected:
	int error_;
	struct addrinfo *addrinfo_;
    ...
};
```

我们想到的的dns output就是addrinfo

## DnsRoutine

其中，如何从DnsInput 到 DnsOutput 的过程是 DnsRoutine

```cpp
class DnsRoutine
{
public:
	static void run(const DnsInput *in, DnsOutput *out);
	static void create(DnsOutput *out, int error, struct addrinfo *ai)
	{
		if (out->addrinfo_)
			freeaddrinfo(out->addrinfo_);

		out->error_ = error;
		out->addrinfo_ = ai;
	}

private:
	static void run_local_path(const std::string& path, DnsOutput *out);
};

```

## DnsRoutine::run

其中比较重要的就是这个run，最终是在这里通过getaddrinfo 通过host, port 获取到addrino

```cpp
void DnsRoutine::run(const DnsInput *in, DnsOutput *out)
{
	if (!in->host_.empty() && in->host_[0] == '/')
	{
		run_local_path(in->host_, out);
		return;
	}

	struct addrinfo hints = {
#ifdef AI_ADDRCONFIG
		.ai_flags    = AI_ADDRCONFIG,
#endif
		.ai_family   = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM
	};
	char port_str[PORT_STR_MAX + 1];

	snprintf(port_str, PORT_STR_MAX + 1, "%u", in->port_);
	out->error_ = getaddrinfo(in->host_.c_str(),
							  port_str,
							  &hints,
							  &out->addrinfo_);
}

```

这里如果是local path，以'/'开头，那么就调用`run_local_path`，我们暂时先不管