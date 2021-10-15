# # workflow 源码解析 10 : dns cache

项目源码 : https://github.com/sogou/workflow

更加详细的源码注释可看 : https://github.com/chanchann/workflow_annotation

相关文档 : https://github.com/sogou/workflow/blob/master/docs/about-conditional.md

注意此处的cache代码都用的是新版代码来分析(更合理简洁)

## 结构组成

```cpp
using HostPort = std::pair<std::string, unsigned short>;
<host, port>

using DnsHandle = LRUHandle<HostPort, DnsCacheValue>;
```

我们先来看看DnsCacheValue

## DnsCacheValue

```cpp
struct DnsCacheValue
{
	struct addrinfo *addrinfo;
	int64_t confident_time;
	int64_t expire_time;
};
```

存的一个是addrinfo和还有超时时间

## LRUHandle

首先存的是个kv结构

其次双向链表串着

还有个引用计数

```cpp
template<typename KEY, typename VALUE>
class LRUHandle
{
public:
	VALUE value;

private:
	LRUHandle():
		prev(NULL),
		next(NULL),
		ref(0)
	{}

	LRUHandle(const KEY& k, const VALUE& v):
		value(v),
		key(k),
		prev(NULL),
		next(NULL),
		ref(0),
		in_cache(false)
	{}
  ...

	KEY key;
	LRUHandle *prev;
	LRUHandle *next;
	int ref;
	bool in_cache;
  ...
};
```

## LRUCache<HostPort, DnsCacheValue, ValueDeleter> cache_pool_;

dns cache的所有方法都是对这个进行操作

这里的ValueDeleter我们等到后面用到再看

## LRUCache

```cpp
template<typename KEY, typename VALUE, class ValueDeleter>
class LRUCache
{
protected:
typedef LRUHandle<KEY, VALUE>			Handle;
typedef std::map<KEY, Handle*>			Map;
typedef typename Map::iterator			MapIterator;
typedef typename Map::const_iterator	MapConstIterator;

public:
	// Remove all cache that are not actively in use.
	void prune();

	// release handle by get/put
	void release(Handle *handle);
	void release(const Handle *handle);

	// get handler
	// Need call release when handle no longer needed
	const Handle *get(const KEY& key);

	// put copy
	// Need call release when handle no longer needed
	const Handle *put(const KEY& key, VALUE value);

	// delete from cache, deleter delay called when all inuse-handle release.
	void del(const KEY& key);

private:
	void list_remove(Handle *node);

	void list_append(Handle *list, Handle *node);

	void ref(Handle *e);

	void unref(Handle *e);
	void erase_node(Handle *e);

	// default max_size=0 means no-limit cache
	// max_size means max cache number of key-value pairs
	size_t max_size_;   
	size_t size_;

  // typedef LRUHandle<HostPort, DnsCacheValue>			Handle;
	Handle not_use_;  
	Handle in_use_;   // todo : 啥有了not_use_还要in_use_
	Map cache_map_;   // typedef std::map<HostPort, LRUHandle<HostPort, DnsCacheValue> *>	Map;
  // todo : 感觉这个数据结构有点重复？

	ValueDeleter value_deleter_;
};
```

因为dns cache就是在操作这个数据结构，所以我们重点分析一番

这也是一个生产者消费者模型，必然先分析一下get/put 操作

## get

```cpp
// get handler
// Need call release when handle no longer needed
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

获取到这个cache，就是找到就ref，然后返回就可，朴实无华

## put

```cpp
// put copy
// Need call release when handle no longer needed
const Handle *put(const KEY& key, VALUE value)
{
	Handle *e = new Handle(key, value);

	e->ref = 1;   // 1 表示不能删除
	size_++;
	e->in_cache = true;
	e->ref++;   // +1 ，表示有一个人在用
	list_append(&in_use_, e);
	MapIterator it = cache_map_.find(key);
	// 如果重复put的话
	if (it != cache_map_.end())
	{
		erase_node(it->second);   // 重复时，之前的移除掉
		it->second = e;   // 换上我们新串入的
	}
	else
		cache_map_[key] = e;  // 存入cache中

	if (max_size_ > 0)   // 如果他有个max_size_数值，那我们需要剔除一些了
	{
		// 当size_ 多了，而且not_use_队列不为空，那么我们从not_use_中去剔除
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

## 这里涉及链表基本操作

1. list_append

给list往后添加一个node

同理，list_remove就是从list中拿掉

这两个图画一下图便知道如何操作

这里不仅是双向，而且是循环的链表结构

![list_append](./pics/appen_list.jpeg)

2. erase_node

这个就是把node删掉(从链表移除，设置为不在cache中，unref)

```cpp	
void erase_node(Handle *e)
{
	assert(e->in_cache);
	list_remove(e);
	e->in_cache = false;
	size_--;
	unref(e);
}
```

## ref / unref

```cpp
void ref(Handle *e)
{
	// 
	if (e->in_cache && e->ref == 1)
	{
		list_remove(e);
		list_append(&in_use_, e);
	}

	e->ref++;
}
```

```cpp
void unref(Handle *e)
{
	assert(e->ref > 0);
	e->ref--;
	if (e->ref == 0)   // 当为0的时候，才是真正去删除掉
	{
		assert(!e->in_cache);
		value_deleter_(e->value);
		delete e;
	}
	// 如果减少到1了，说明没人拿来用了，并且还在cache中，那么就加入not_use_
	// 此处和上面的ref对应
	else if (e->in_cache && e->ref == 1)  
	{
		list_remove(e);
		list_append(&not_use_, e);
	}
}
```


## ~LRUCache() 

因为是结束才调用，所以我们放在最后来说






