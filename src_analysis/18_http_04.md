# workflow 源码解析 09 : CommSched

更加详细的源码注释可看 : https://github.com/chanchann/workflow_annotation

## UML 图

![CommSchedObject](https://github.com/chanchann/workflow_annotation/blob/main/src_analysis/pics/CommSchedObject.png?raw=true)

## CommSchedObject

先从父类说起，主要是看load，看起来就是个负载均衡的作用

还有个acquire纯虚函数需要实现，就是负载均衡拿出一个object去schedule

```cpp
class CommSchedObject
{
public:
	size_t get_max_load() { return this->max_load; }
	size_t get_cur_load() { return this->cur_load; }

private:
	virtual CommTarget *acquire(int wait_timeout) = 0;

protected:
	size_t max_load;
	size_t cur_load;
};

```

CommSchedObject有两个子类，一个CommSchedGroup，一个CommSchedTarget

从名字可以看出，一个是负载均衡的一个组，一个是xxx

### CommSchedGroup

```cpp
class CommSchedGroup : public CommSchedObject
{
public:
	int init();
	void deinit();
	int add(CommSchedTarget *target);
	int remove(CommSchedTarget *target);

private:
	virtual CommTarget *acquire(int wait_timeout); /* final */

private:
	CommSchedTarget **tg_heap;
	int heap_size;
	int heap_buf_size;
	int wait_cnt;
	pthread_mutex_t mutex;
	pthread_cond_t cond;

private:
	static int target_cmp(CommSchedTarget *target1, CommSchedTarget *target2);
	void heapify(int top);
	void heap_adjust(int index, int swap_on_equal);
	int heap_insert(CommSchedTarget *target);
	void heap_remove(int index);
	friend class CommSchedTarget;
};
```

可以看出，Group是一个heap来组织负载均衡的

`CommSchedTarget **tg_heap;` 是一个装 `CommSchedTarget *` 的数组

对外暴露两个接口，add，remove

### target_cmp

我们首先看是怎么比较两个CommSchedTarget

```cpp
int CommSchedGroup::target_cmp(CommSchedTarget *target1,
							   CommSchedTarget *target2)
{
	size_t load1 = target1->cur_load * target2->max_load;
	size_t load2 = target2->cur_load * target1->max_load;

	if (load1 < load2)
		return -1;
	else if (load1 > load2)
		return 1;
	else
		return 0;
}
```

### CommSchedGroup::add
 
```cpp

int CommSchedGroup::add(CommSchedTarget *target)
{
	pthread_mutex_lock(&target->mutex);
	pthread_mutex_lock(&this->mutex);
    
	if (target->group == NULL && target->wait_cnt == 0)
	{
		if (this->heap_insert(target) >= 0)
		{
			target->group = this;
			this->max_load += target->max_load;
			this->cur_load += target->cur_load;
			if (this->wait_cnt > 0 && this->cur_load < this->max_load)
				pthread_cond_signal(&this->cond);

			ret = 0;
		}
	}
	else if (target->group == this)
		errno = EEXIST;
	else if (target->group)
		errno = EINVAL;
	else
		errno = EBUSY;

	pthread_mutex_unlock(&this->mutex);
	pthread_mutex_unlock(&target->mutex);
	return ret;
}
```

```cpp
int CommSchedGroup::remove(CommSchedTarget *target)
{
	pthread_mutex_lock(&target->mutex);
	pthread_mutex_lock(&this->mutex);
	if (target->group == this && target->wait_cnt == 0)
	{
		this->heap_remove(target->index);
		this->max_load -= target->max_load;
		this->cur_load -= target->cur_load;
		target->group = NULL;
		ret = 0;
	}
	else if (target->group != this)
		errno = ENOENT;
	else
		errno = EBUSY;

	pthread_mutex_unlock(&this->mutex);
	pthread_mutex_unlock(&target->mutex);
	return ret;
}
```