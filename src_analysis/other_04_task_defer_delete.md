# workflow杂记04 : 分析Defer deleting server task to increase performance

分析代码改动地址 : https://github.com/sogou/workflow/commit/a7a89d5b5a4426fa5bf85f2d82f7385fd45dd9e3

项目源码 : https://github.com/sogou/workflow

更加详细的源码注释可看 : https://github.com/chanchann/workflow_annotation

### 代码对比

```cpp
// 老版代码
// WFServerTask
virtual SubTask *done()
{
    SeriesWork *series = series_of(this);

    if (this->state == WFT_STATE_SYS_ERROR && this->error < 0)
    {
        this->state = WFT_STATE_SSL_ERROR;
        this->error = -this->error;
    }

    if (this->callback)
        this->callback(this);

    delete this;
    return series->pop();
}
```

```cpp
// 新版代码
// WFServerTask
virtual SubTask *done()
{
    SeriesWork *series = series_of(this);

    if (this->state == WFT_STATE_SYS_ERROR && this->error < 0)
    {
        this->state = WFT_STATE_SSL_ERROR;
        this->error = -this->error;
    }

    if (this->callback)
        this->callback(this);

    /* Defer deleting the task. */
    return series->pop();
}
```

对比下，可见我们这里执行完了callback不会马上delete掉task。

而在什么时候删除呢

```cpp
class Series : public SeriesWork
{
public:
    Series(WFServerTask<REQ, RESP> *task) :
        SeriesWork(&task->processor, nullptr)
    {
        this->set_last_task(task);
        this->task = task;
    }

    virtual ~Series()
    {
        delete this->task;
    }

    WFServerTask<REQ, RESP> *task;
};
```

在Series析构的时候delete

对比下旧版代码

```cpp
// 旧版代码
class Series : public SeriesWork
{
public:
    Series(WFServerTask<REQ, RESP> *task) :
        SeriesWork(&task->processor, nullptr)
    {
        this->set_last_task(task);
        this->service = task->service;
        this->service->incref();
    }

    virtual ~Series()
    {
        this->callback = nullptr;
        this->service->decref();
    }

    CommService *service;
};
```

这里ref就是引用计数，service需要引用计数到0才解绑完成,相当于手动shared_ptr。

```cpp
void decref()
{
    if (__sync_sub_and_fetch(&this->ref, 1) == 0)
        this->handle_unbound();
}
```

```cpp
void WFServerBase::handle_unbound()
{
	this->mutex.lock();
	this->unbind_finish = true;
	this->cond.notify_one();
	this->mutex.unlock();
}
```

这里notify了，我们看看哪里在wait

```cpp
void WFServerBase::wait_finish()
{
	SSL_CTX *ssl_ctx = this->get_ssl_ctx();
	std::unique_lock<std::mutex> lock(this->mutex);

	while (!this->unbind_finish)
		this->cond.wait(lock);

	this->deinit();
	this->unbind_finish = false;
	lock.unlock();
	if (ssl_ctx)
		SSL_CTX_free(ssl_ctx);
}
```

到这里就deinit结束。

这里相当于把他的生命周期延长到Series结束才终结。

而我们新版代码中，直到Series结束才delete，就减少了这两个用于延长生命周期的原子操作。

