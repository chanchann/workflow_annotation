// 模拟 https://github.com/sogou/workflow/issues/135

#include <signal.h>

#include <string>
#include <workflow/HttpMessage.h>
#include <workflow/HttpUtil.h>
#include <workflow/WFServer.h>
#include <workflow/WFHttpServer.h>
#include <workflow/WFFacilities.h>
#include <spdlog/spdlog.h>

#include <arpa/inet.h>

// http server端模拟慢请求，不要在process里调用sleep函数，最好使用定时器任务
void process(WFHttpTask *server_task)
{
    server_task->get_resp()->append_output_body("Hello World!");
    // 延迟5秒返回的http server。等待时间不占用线程。
    series_of(server_task)->push_back(WFTaskFactory::create_timer_task(5 * 1000 * 1000, nullptr));
}

static WFFacilities::WaitGroup wait_group(1);

void sig_handler(int signo)
{
    wait_group.done();
}

int main()
{
    signal(SIGINT, sig_handler);

    WFHttpServer server(process);
    uint16_t port = 8888;

    if (server.start(port) == 0)
    {
        spdlog::info("server start at : {}", port);
        wait_group.wait();
        server.stop();
    }
    else
    {
        spdlog::critical("Cannot start server");
        exit(1);
    }

    return 0;
}