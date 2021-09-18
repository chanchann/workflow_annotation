// 转发服务器的构建和series上下文的使用

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
}

void process(WFHttpTask *server_task)
{
    HttpRequest *req = server_task->get_req();
    std::string url("http://www.baidu.com");
    url = url + req->get_request_uri();

    // 向series里push_back一个http client任务，并且把这个http任务返回的结果写进server的回复消息
    WFHttpTask *client_task = WFTaskFactory::create_http_task(url, 3, 2, callback);
    *client_task->get_req() = std::move(*req);  // 把req转发给后端
    client_task->get_req()->set_header_pair("Host", "www.baidu.com");   // 少这个会403 forbidden

    SeriesWork *series = series_of(server_task);
    // 每个运行中的任务一点处于某个任务流（series）里，同一个series上的任务，可以通过series context共享上下文。
    // series上下文用来存放server task, 以便在client任务完成之后填写回复消息
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