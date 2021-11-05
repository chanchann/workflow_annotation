## drogon router

首先看个example

```cpp
app().registerHandler(
    "/",
    [](const HttpRequestPtr &req,
        std::function<void(const HttpResponsePtr &)> &&callback) {
        bool loggedIn =
            req->session()->getOptional<bool>("loggedIn").value_or(false);
        HttpResponsePtr resp;
        if (loggedIn == false)
            resp = HttpResponse::newHttpViewResponse("LoginPage");
        else
            resp = HttpResponse::newHttpViewResponse("LogoutPage");
        callback(resp);
    });
```

WF_ROUT("/path/<int>/<double>", [](int a, double b) {
    std::cout << a << std::endl;
    std::cout << b << std::endl;
}



