#! https://zhuanlan.zhihu.com/p/448011565
# workflow杂记01 : 分析一次cache中lock的改动

分析代码改动地址 : https://github.com/sogou/workflow/commit/b1db18cd39566ef90d4962f29fd080a6e648b081

项目源码 : https://github.com/sogou/workflow

更加详细的源码注释可看 : https://github.com/chanchann/workflow_annotation

# 首先来看DNS cache

我们可见，dns cache是一个单例，在WFGlobal里

```cpp
DnsCache *WFGlobal::get_dns_cache()
{
	return __DnsCache::get_instance()->get_dns_cache();
}
```

dns cache只在这2处被调用，一个是取出，一个是放

```cpp

void WFResolverTask::dispatch()
{
	if (dns_cache_level_ != DNS_CACHE_LEVEL_0)
	{
		auto *dns_cache = WFGlobal::get_dns_cache();
		const DnsCache::DnsHandle *addr_handle = NULL;

		switch (dns_cache_level_)
		{
		case DNS_CACHE_LEVEL_1:
			addr_handle = dns_cache->get_confident(host_, port_);
			break;

		case DNS_CACHE_LEVEL_2:
			addr_handle = dns_cache->get_ttl(host_, port_);
			break;

		case DNS_CACHE_LEVEL_3:
			addr_handle = dns_cache->get(host_, port_);
			break;

		default:
			break;
		}

		if (addr_handle)
		{
            ...
			dns_cache->release(addr_handle);
		}
	}
    ...
}
```

```cpp

void WFResolverTask::dns_callback_internal(DnsOutput *dns_out,
										   unsigned int ttl_default,
										   unsigned int ttl_min)
{
	if (dns_error)
	{
        ...
	}
	else
	{
		auto *dns_cache = WFGlobal::get_dns_cache();
		struct addrinfo *addrinfo = dns_out->move_addrinfo();
		const DnsCache::DnsHandle *addr_handle;

		addr_handle = (host_, port_, addrinfo,
									 (unsigned int)ttl_default,
									 (unsigned int)ttl_min);
        ...

		dns_cache->release(addr_handle);
	}
}
```

## 旧版代码

https://github.com/chanchann/workflow_annotation/blob/main/workflow/src/manager/DnsCache.h

这里的代码我们成员变量中有个mutex_, 只在.cc中的put中使用，这个一看就是两次加锁了

cache其实就是一个LRU结构 

```cpp
LRUCache<HostPort, DnsCacheValue, ValueDeleter> cache_pool_;
```

看这一块锁的改变，其实就是在外部锁还是内部锁住。

旧版代码在外部没锁，只能去看下LRU中是怎么操作的

## 新版代码

加锁就一个原则，因为是全局的一个单例，可能在多个线程里会用到，所以当我们改变一个数据结构，比如STL中一些非线程安全的结构时，需要加锁来保证线程安全

```cpp
class DnsCache
{
public:
	using HostPort = std::pair<std::string, unsigned short>;
	using DnsHandle = LRUHandle<HostPort, DnsCacheValue>;

public:
	// release handle by get/put
	void release(DnsHandle *handle)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		cache_pool_.release(handle);  // release中 要unref所以加锁
	}

	void release(const DnsHandle *handle)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		cache_pool_.release(handle);  // release中 要unref所以加锁
	}

	// get handler
	// Need call release when handle no longer needed
	//Handle *get(const KEY &key);
	const DnsHandle *get(const HostPort& host_port)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return cache_pool_.get(host_port);   // get 要 ref，find所以加锁
	}

	const DnsHandle *get(const std::string& host, unsigned short port)
	{
		return get(HostPort(host, port));    // 调用其他函数，在最里面那个get加锁
	}

	const DnsHandle *get(const char *host, unsigned short port)
	{
		return get(std::string(host), port);  // 调用其他函数，在最里面那个get加锁
	}

	const DnsHandle *get_ttl(const HostPort& host_port)
	{
		return get_inner(host_port, GET_TYPE_TTL);   // 调用其他函数，在get_inner加锁
	}

	const DnsHandle *get_ttl(const std::string& host, unsigned short port)
	{
		return get_ttl(HostPort(host, port));   // 调用其他函数，在最里面那个get_inner加锁
	}

	const DnsHandle *get_ttl(const char *host, unsigned short port)
	{
		return get_ttl(std::string(host), port);    // 调用其他函数，在最里面那个get_inner加锁
	}

	const DnsHandle *get_confident(const HostPort& host_port)
	{
		return get_inner(host_port, GET_TYPE_CONFIDENT);    // 调用其他函数，在get_inner加锁
	}

	const DnsHandle *get_confident(const std::string& host, unsigned short port)
	{
		return get_confident(HostPort(host, port));   // 调用其他函数，在最里面那个get_inner加锁
	}

	const DnsHandle *get_confident(const char *host, unsigned short port)
	{
		return get_confident(std::string(host), port);   // 调用其他函数，在最里面那个get_inner加锁
	}

	const DnsHandle *put(const HostPort& host_port,
						 struct addrinfo *addrinfo,
						 unsigned int dns_ttl_default,
						 unsigned int dns_ttl_min);

	const DnsHandle *put(const std::string& host,
						 unsigned short port,
						 struct addrinfo *addrinfo,
						 unsigned int dns_ttl_default,
						 unsigned int dns_ttl_min)
	{
        // 调用上面那个put，在里面加锁
		return put(HostPort(host, port), addrinfo, dns_ttl_default, dns_ttl_min);
	}

	const DnsHandle *put(const char *host,
						 unsigned short port,
						 struct addrinfo *addrinfo,
						 unsigned int dns_ttl_default,
						 unsigned int dns_ttl_min)
	{
        // 调用最上面那个put，在里面加锁
		return put(std::string(host), port, addrinfo, dns_ttl_default, dns_ttl_min);
	}

	// delete from cache, deleter delay called when all inuse-handle release.
	void del(const HostPort& key)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		cache_pool_.del(key);   
	}

	void del(const std::string& host, unsigned short port)
	{
		del(HostPort(host, port));  // 调用其他函数，在最里面那个del加锁
	}

	void del(const char *host, unsigned short port)
	{
		del(std::string(host), port);  // 调用其他函数，在最里面那个del加锁
	}

	std::mutex mutex_;

	LRUCache<HostPort, DnsCacheValue, ValueDeleter> cache_pool_;
};
```

然后到此，新老代码都没毛病，只是加锁的粒度不同。

我们再次依次仔细对比

## release

```cpp
// 旧版

void release(DnsHandle *handle)
{
    cache_pool_.release(handle);
}

// 放在里面加锁
void release(Handle *handle)
{
    if (handle)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        unref(handle);
    }
}
```

```cpp
// 新版
void release(DnsHandle *handle)
{
    std::lock_guard<std::mutex> lock(mutex_);
    cache_pool_.release(handle);
}

// 直接在外面加锁了

// release handle by get/put
void release(Handle *handle)
{
    unref(handle);
}
```

## get

```cpp
// 旧版
const DnsHandle *get(const HostPort& host_port)
{
    return cache_pool_.get(host_port);
}

// 在里面加锁
const Handle *get(const KEY& key)
{
    std::lock_guard<std::mutex> lock(mutex_);
    MapConstIterator it = cache_map_.find(key);

    if (it != cache_map_.end())
    {
        ref(it->second);
        return it->second;
    }

    return NULL;
}
```

```cpp
// 新版
const DnsHandle *get(const HostPort& host_port)
{
    std::lock_guard<std::mutex> lock(mutex_);
    return cache_pool_.get(host_port);
}

// 在外面直接加锁
const Handle *get(const KEY& key)
{
    MapConstIterator it = cache_map_.find(key);

    if (it != cache_map_.end())
    {
        ref(it->second);
        return it->second;
    }

    return NULL;
}
```

## get_inner

```cpp
// 旧版

const DnsCache::DnsHandle *DnsCache::get_inner(const HostPort& host_port, int type)
{

	const DnsHandle *handle = cache_pool_.get(host_port);  // 此处在get内部加锁

	if (handle)
	{
		int64_t cur_time = GET_CURRENT_SECOND;

		switch (type)
		{
		case GET_TYPE_TTL:
			if (cur_time > handle->value.expire_time)
			{
				std::lock_guard<std::mutex> lock(mutex_);

				if (cur_time > handle->value.expire_time)
				{
					const_cast<DnsHandle *>(handle)->value.expire_time += TTL_INC;
					cache_pool_.release(handle); 
                    // !!! problems : 加了2次锁，因为他在release内部也加了锁，其实这括号中的锁应该不需要要
					return NULL;
				}
			}

			break;

		case GET_TYPE_CONFIDENT:
			if (cur_time > handle->value.confident_time)
			{
				std::lock_guard<std::mutex> lock(mutex_);

				if (cur_time > handle->value.confident_time)
				{
					const_cast<DnsHandle *>(handle)->value.confident_time += CONFIDENT_INC;
					cache_pool_.release(handle);
                    // !!! problems : 加了2次锁，因为他在release内部也加了锁，其实这括号中的锁应该不需要要
					return NULL;
				}
			}

			break;

		default:
			break;
		}
	}

	return handle;
}
```

```cpp
// 新版

const DnsCache::DnsHandle *DnsCache::get_inner(const HostPort& host_port, int type)
{
	int64_t cur_time = GET_CURRENT_SECOND;
	std::lock_guard<std::mutex> lock(mutex_);
	const DnsHandle *handle = cache_pool_.get(host_port);

	if (handle)
	{
		switch (type)
		{
		case GET_TYPE_TTL:
			if (cur_time > handle->value.expire_time)   // 这个概率非常小
			{
				const_cast<DnsHandle *>(handle)->value.expire_time += TTL_INC;
				cache_pool_.release(handle);
				return NULL;
			}

			break;

		case GET_TYPE_CONFIDENT:
			if (cur_time > handle->value.confident_time)
			{
				const_cast<DnsHandle *>(handle)->value.confident_time += CONFIDENT_INC;
				cache_pool_.release(handle);
				return NULL;
			}

			break;

		default:
			break;
		}
	}

	return handle;
}
```

## put

```cpp
// 旧版代码
const DnsCache::DnsHandle *DnsCache::put(const HostPort& host_port,
										 struct addrinfo *addrinfo,
										 unsigned int dns_ttl_default,
										 unsigned int dns_ttl_min)
{
    ...

	std::lock_guard<std::mutex> lock(mutex_);
	return cache_pool_.put(host_port, {addrinfo, confident_time, expire_time});  // 这里面又是加锁，又加了两次锁
}

// 而且下面加锁的逻辑比较乱
const Handle *put(const KEY& key, VALUE value)
{
    Handle *e = new Handle(key, value);

    e->ref = 1;
    std::lock_guard<std::mutex> lock(mutex_);

    size_++;
    e->in_cache = true;
    e->ref++;
    list_append(&in_use_, e);
    MapIterator it = cache_map_.find(key);
    if (it != cache_map_.end())
    {
        erase_node(it->second);
        it->second = e;
    }
    else
        cache_map_[key] = e;

    if (max_size_ > 0)
    {
        while (size_ > max_size_ && not_use_.next != &not_use_)
        {
            Handle *old = not_use_.next;

            assert(old->ref == 1);
            cache_map_.erase(old->key);
            erase_node(old);
        }
    }

    return e;
}

```

```cpp
// 新版代码

const DnsCache::DnsHandle *DnsCache::put(const HostPort& host_port,
										 struct addrinfo *addrinfo,
										 unsigned int dns_ttl_default,
										 unsigned int dns_ttl_min)
{   
    ...

	std::lock_guard<std::mutex> lock(mutex_);
	return cache_pool_.put(host_port, {addrinfo, confident_time, expire_time});
}

// 同样也是在外面lock住，那里面就不用加锁了，直接全部锁住
// 可见完全没锁
const Handle *put(const KEY& key, VALUE value)
{
    Handle *e = new Handle(key, value);

    e->ref = 1;
    size_++;
    e->in_cache = true;
    e->ref++;
    list_append(&in_use_, e);
    MapIterator it = cache_map_.find(key);
    if (it != cache_map_.end())
    {
        erase_node(it->second);
        it->second = e;
    }
    else
        cache_map_[key] = e;

    if (max_size_ > 0)
    {
        while (size_ > max_size_ && not_use_.next != &not_use_)
        {
            Handle *old = not_use_.next;

            assert(old->ref == 1);
            cache_map_.erase(old->key);
            erase_node(old);
        }
    }

    return e;
}
```

## del

```cpp
// 旧版
void del(const HostPort& key)
{
    cache_pool_.del(key);
}

// 和上面get/release一样，加锁任务给LRU的del了
void del(const KEY& key)
{
    std::lock_guard<std::mutex> lock(mutex_);
    MapConstIterator it = cache_map_.find(key);

    if (it != cache_map_.end())
    {
        Handle *node = it->second;

        cache_map_.erase(it);
        erase_node(node);
    }
}
```

```cpp
// 新版
void del(const HostPort& key)
{
    std::lock_guard<std::mutex> lock(mutex_);
    cache_pool_.del(key);
}

// 在外锁住了
// 内部完全没锁
void del(const KEY& key)
{
    MapConstIterator it = cache_map_.find(key);

    if (it != cache_map_.end())
    {
        Handle *node = it->second;

        cache_map_.erase(it);
        erase_node(node);
    }
}
```

## 析构函数

```cpp
// 旧版

~LRUCache()
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Error if caller has an unreleased handle
    assert(in_use_.next == &in_use_);
    for (Handle *e = not_use_.next; e != &not_use_; )
    {
        Handle *next = e->next;

        assert(e->in_cache);
        e->in_cache = false;
        assert(e->ref == 1);// Invariant for not_use_ list.
        unref(e);
        e = next;
    }
}
```

```cpp
// 新版
~LRUCache()
{
    // Error if caller has an unreleased handle
    assert(in_use_.next == &in_use_);
    for (Handle *e = not_use_.next; e != &not_use_; )
    {
        Handle *next = e->next;

        assert(e->in_cache);
        e->in_cache = false;
        assert(e->ref == 1);// Invariant for not_use_ list.
        unref(e);
        e = next;
    }
}
```

这个对于我的理解是，我们的cache是个单例, LRU与其是组合模式，生命周期相同

当析构的时候，在一个线程完成，不存在多线程抢着析构的情况，所以这里不需要加锁

## 总结

老版代码中，加锁思路不清晰，导致了有几处重复加多把锁的情况

新版代码一个原则，在外部加锁，让LRU这个基本数据结构内部的操作不考虑多线程竞争，这样思路就非常清晰明了

从直觉上来说，我们LRU作为一个基本的数据结构，也最好不要考虑加锁，所有的加锁职责交给cache这个我们需要的实体，因为我们程序竞争的是cache而不是LRU

而且我们在外部加锁的话，仔细看其实锁的粒度并没有什么变化，比如get_inner中，if条件中的概率是非常小的，所以大部分时间锁的粒度还是非常小。

