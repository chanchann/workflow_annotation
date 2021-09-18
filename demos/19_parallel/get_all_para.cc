/*
如何汇总一个ParallelWork的结果

https://github.com/sogou/workflow/issues/140
*/

#include <workflow/WFTaskFactory.h>
#include <workflow/WFHttpServer.h>
#include <workflow/WFFacilities.h>
#include <signal.h>
#include <spdlog/spdlog.h>
#include <mutex>

using namespace protocol;

std::mutex mutex;

struct series_context
{
    std::vector<HttpResponse> resp_list;
    WFHttpTask *server_task;
};

void fetch_callback(WFHttpTask *fetch_task)
{
    spdlog::info("fetch cb start");
    SeriesWork *series_sub = series_of(fetch_task);
    ParallelWork *pwork = static_cast<ParallelWork *>(series_sub->get_context());

    SeriesWork *series = series_of(pwork);
    series_context *context = static_cast<series_context *>(series->get_context());

    HttpResponse *resp = fetch_task->get_resp();

    spdlog::info("save resp in context");

    std::lock_guard<std::mutex> lock(mutex);
    context->resp_list.emplace_back(std::move(*resp));
}

void parallel_callback(const ParallelWork *pwork)
{
    SeriesWork *series = series_of(pwork);
    series_context *context = static_cast<series_context *>(series->get_context());
    const void *body;
    size_t size;
    for (auto &resp : context->resp_list)
    {
        if (resp.get_parsed_body(&body, &size))
        {
            spdlog::info("resp size : {}", size);
            // fwrite(body, 1, size, stdout);  // for test
            context->server_task->get_resp()->append_output_body_nocopy(body, size);
        }
    }
    spdlog::info("All series in this parallel have done");
}

ParallelWork *create_fetch_paralell()
{
    std::vector<std::string> urls =
        {
            "http://www.baidu.com",
            "http://www.bing.com",
            "http://www.sogo.com"};

    ParallelWork *pwork = Workflow::create_parallel_work(parallel_callback);
    for (auto &url : urls)
    {
        WFHttpTask *task = WFTaskFactory::create_http_task(url,
                                                           4,
                                                           2,
                                                           fetch_callback);
        SeriesWork *series = Workflow::create_series_work(task, nullptr);
        series->set_context(pwork);
        pwork->add_series(series);
    }
    return pwork;
}

void process(WFHttpTask *server_task)
{
    HttpRequest *req = server_task->get_req();
    if (strcmp(req->get_request_uri(), "/test") == 0)
    {
        ParallelWork *pwork = create_fetch_paralell();
        SeriesWork *series = series_of(server_task);

        series_context *context = new series_context;
        context->server_task = server_task;
        
        series->set_context(context);
        series->push_back(pwork);
        server_task->set_callback([&context](WFHttpTask *)
                                {
                                    spdlog::info("delete context");
                                    delete context;
                                });
        return;
    }
    else
    {
        server_task->get_resp()->append_output_body("<html>server other response</html>");
        return;
    }

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