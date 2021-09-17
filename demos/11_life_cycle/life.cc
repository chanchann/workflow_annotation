// 生命周期探究

#include <spdlog/spdlog.h>
#include <workflow/WFTaskFactory.h>
#include <workflow/WFHttpServer.h>
#include <workflow/WFFacilities.h>
#include <signal.h>

using namespace protocol;

void callback(WFHttpTask *client_task)
{
    HttpResponse *resp = client_task->get_resp();
    const void *body;
    size_t size;

    if (resp->get_parsed_body(&body, &size))
        resp->append_output_body_nocopy(body, size);

    SeriesWork *series = series_of(client_task);
    WFHttpTask *server_task = static_cast<WFHttpTask *>(series->get_context());
    *server_task->get_resp() = std::move(*resp); 

    // process是处理函数而不是server的callback
    // 所以process之后还能操作server task
    // 此处我们才设置了server的callback， 调用时机是server回复完成
    // server回复成功指的就是把数据完全写入TCP buffer
    // 而在这个callback之后，server task的生命周期也就结束了
    server_task->set_callback([](WFHttpTask *server_task) {
        int state = server_task->get_state();
        int error = server_task->get_error();
        if (state == WFT_STATE_SUCCESS)
            spdlog::info("Reply Success!");
        else
            spdlog::info("Reply failed! state = {}, error = {}", state, error);
    });
}

void process(WFHttpTask *server_task)
{
    HttpRequest *req = server_task->get_req();
    std::string url("http://www.baidu.com"); 
    url = url + req->get_request_uri();

    WFHttpTask *client_task = WFTaskFactory::create_http_task(url, 3, 2, callback);
    client_task->get_req()->set_header_pair("Host", "www.baidu.com");

    SeriesWork *series = series_of(server_task);    
    series->set_context(server_task);
    series->push_back(client_task);
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

    if (server.start(8888) == 0)
    {
        wait_group.wait();
        server.stop();
    }

    return 0;
}
