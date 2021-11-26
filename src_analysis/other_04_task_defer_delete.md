## 分析Defer deleting server task to increase performance

https://github.com/sogou/workflow/commit/a7a89d5b5a4426fa5bf85f2d82f7385fd45dd9e3

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

这里ref就是引用计数，service需要引用计数到0才解绑完成，connection要ref=0才能释放。因为异步环境下，连接随时可能被关闭，所有需要引用计数，相当于手动shared_ptr。这里相当于把他的生命周期延长到Series结束。

而我们新版代码中，知道Series结束才delete，就减少了这两个用于延长生命周期的原子操作。

