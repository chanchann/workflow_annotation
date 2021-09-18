#include <string.h>
#include <atomic>
#include <workflow/WFHttpServer.h>

// 命令行中用 curl -i "http://127.0.0.1/test"
// https://github.com/sogou/workflow/issues/89
// more elegant way :

int main()
{
    WFHttpServer server([&server](WFHttpTask *task) { // 这里的写法相当于一个defer作用
        if (strcmp(task->get_req()->get_request_uri(), "/stop") == 0)
        {
            static std::atomic<int> flag;
            
            if (flag++ == 0) // shutdown只能调用一次，因此我们用了原子变量保护
                server.shutdown();

            task->get_resp()->append_output_body("<html>server stop</html>");
            return;
        }
        else if (strcmp(task->get_req()->get_request_uri(), "/test") == 0)
        {
            task->get_resp()->append_output_body("<html>server test</html>");
            return;
        }

        /* Server’s logic */
        //  ....
    });

    if (server.start(8888) == 0)
        server.wait_finish();

    return 0;
}

/*
// WFServer.h
class WFServerBase : protected CommService
{
    ...
public:
    void stop()
    {
        this->shutdown();
        this->wait_finish();
    }

    void shutdown();
    void wait_finish();
};

Server的stop方法无非就是shutdown+wait_finish，上述的方法就是将关停和等待结束分别在两个线程里调用

显然在process里直接调用stop则是一种错误。
*/