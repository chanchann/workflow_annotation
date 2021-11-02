## 数据结构

主要就是两个

```cpp
// method => http_method_handler
typedef std::list<http_method_handler>                                  http_method_handlers;
// path => http_method_handlers
typedef std::map<std::string, std::shared_ptr<http_method_handlers>>    http_api_handlers;
```

一个path 对应了一个 http_method_handler 的链表

其中 http_method_handler 有method 和 handler

```cpp
struct http_method_handler {
    http_method         method;
    http_sync_handler   sync_handler;
    http_async_handler  async_handler;
    http_handler        handler;
    http_method_handler(http_method m = HTTP_POST,
                        http_sync_handler s = NULL,
                        http_async_handler a = NULL,
                        http_handler h = NULL)
    {
        method = m;
        sync_handler = std::move(s);
        async_handler = std::move(a);
        handler = std::move(h);
    }
};
```

## Get

```cpp
router.GET("/ping", [](HttpRequest* req, HttpResponse* resp) {
    return resp->String("pong");
});
```

```cpp
void GET(const char* relativePath, http_sync_handler handlerFunc) {
    Handle("GET", relativePath, handlerFunc);
}
```

```cpp
void Handle(const char* httpMethod, const char* relativePath, http_sync_handler handlerFunc) {
    AddApi(relativePath, http_method_enum(httpMethod), handlerFunc, NULL, NULL);
}
```

```cpp
void HttpService::AddApi(const char* path, http_method method, http_sync_handler sync_handler, http_async_handler async_handler, http_handler handler) {
    std::shared_ptr<http_method_handlers> method_handlers = NULL;
    auto iter = api_handlers.find(path);
    if (iter == api_handlers.end()) {
        // add path
        method_handlers = std::shared_ptr<http_method_handlers>(new http_method_handlers);
        api_handlers[path] = method_handlers;
    }
    else {
        method_handlers = iter->second;
    }
    for (auto iter = method_handlers->begin(); iter != method_handlers->end(); ++iter) {
        if (iter->method == method) {
            // update
            iter->sync_handler = sync_handler;
            iter->async_handler = async_handler;
            iter->handler = handler;
            return;
        }
    }
    // add
    method_handlers->push_back(http_method_handler(method, sync_handler, async_handler, handler));
}
```

path -> method_handler的链表

然后链表里，如果有当前的method，就赋值上handler，如果没有，就增加一个method-handler

## 拿出handler

我们从httpServer中找，当事件来临，我们怎么处理

on_recv -> FeedRecvData -> HttpRequest

// todo : 这里的http处理之后需要再好好研究研究

```cpp
static void on_recv(hio_t* io, void* _buf, int readbytes) {
    ...
    HttpHandler* handler = (HttpHandler*)hevent_userdata(io);
    // HttpHandler::Init(http_version) -> upgrade ? SwitchHTTP2 / SwitchWebSocket
    // on_recv -> FeedRecvData -> HttpRequest
    // onComplete -> HandleRequest -> HttpResponse -> while (GetSendData) -> send

    HttpHandler::ProtocolType protocol = handler->protocol;
    if (protocol == HttpHandler::UNKNOWN) {
        ...
    }

    int nfeed = handler->FeedRecvData(buf, readbytes);
    ...

    // Server:
    ...
    // Upgrade:
    ...

    int status_code = 200;
    if (parser->IsComplete() && !upgrade) {
        status_code = handler->HandleHttpRequest();
    }

    char* data = NULL;
    size_t len = 0;
    while (handler->GetSendData(&data, &len)) {
        // printf("%.*s\n", (int)len, data);
        if (data && len) {
            hio_write(io, data, len);
        }
    }
    ...
}
```

## HandleHttpRequest

```cpp

int HttpHandler::HandleHttpRequest() {
    // preprocessor -> processor -> postprocessor
    ...

preprocessor:
    ...

processor:
    if (service->processor) {
        status_code = customHttpHandler(service->processor);
    } else {
        status_code = defaultRequestHandler();
    }

postprocessor:
    ...
}
```

## defaultRequestHandler

```cpp

int HttpHandler::defaultRequestHandler() {
    ...

    if (service->api_handlers.size() != 0) {
        service->GetApi(req.get(), &sync_handler, &async_handler, &ctx_handler);
    }

    if (sync_handler) {
        // sync api handler
        status_code = sync_handler(req.get(), resp.get());
    }
    ...
}

```

## GetApi

router对于url的所有功能都在此函数之中了

```cpp

int HttpService::GetApi(HttpRequest* req, http_sync_handler* sync_handler, http_async_handler* async_handler, http_handler* handler) {
    // {base_url}/path?query
    const char* s = req->path.c_str();
    const char* b = base_url.c_str();
    while (*s && *b && *s == *b) {++s;++b;}
    if (*b != '\0') {
        return HTTP_STATUS_NOT_FOUND;
    }
    const char* e = s;
    while (*e && *e != '?') ++e;

    std::string path = std::string(s, e);
    const char *kp, *ks, *vp, *vs;
    bool match;
    for (auto iter = api_handlers.begin(); iter != api_handlers.end(); ++iter) {
        kp = iter->first.c_str();
        vp = path.c_str();
        match = false;
        std::map<std::string, std::string> params;

        while (*kp && *vp) {
            if (kp[0] == '*') {
                // wildcard *
                match = strendswith(vp, kp+1);
                break;
            } else if (*kp != *vp) {
                match = false;
                break;
            } else if (kp[0] == '/' && (kp[1] == ':' || kp[1] == '{')) {
                    // RESTful /:field/
                    // RESTful /{field}/
                    kp += 2;
                    ks = kp;
                    while (*kp && *kp != '/') {++kp;}
                    vp += 1;
                    vs = vp;
                    while (*vp && *vp != '/') {++vp;}
                    int klen = kp - ks;
                    if (*(ks-1) == '{' && *(kp-1) == '}') {
                        --klen;
                    }
                    params[std::string(ks, klen)] = std::string(vs, vp-vs);
                    continue;
            } else {
                ++kp;
                ++vp;
            }
        }

        match = match ? match : (*kp == '\0' && *vp == '\0');

        if (match) {
            auto method_handlers = iter->second;
            for (auto iter = method_handlers->begin(); iter != method_handlers->end(); ++iter) {
                if (iter->method == req->method) {
                    for (auto& param : params) {
                        // RESTful /:field/ => req->query_params[field]
                        req->query_params[param.first] = param.second;
                    }
                    if (sync_handler) *sync_handler = iter->sync_handler;
                    if (async_handler) *async_handler = iter->async_handler;
                    if (handler) *handler = iter->handler;
                    return 0;
                }
            }

            if (params.size() == 0) {
                if (sync_handler) *sync_handler = NULL;
                if (async_handler) *async_handler = NULL;
                if (handler) *handler = NULL;
                return HTTP_STATUS_METHOD_NOT_ALLOWED;
            }
        }
    }
    if (sync_handler) *sync_handler = NULL;
    if (async_handler) *async_handler = NULL;
    if (handler) *handler = NULL;
    return HTTP_STATUS_NOT_FOUND;
}
```
