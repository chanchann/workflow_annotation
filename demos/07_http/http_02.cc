// 依次发起多个请求

#include <workflow/Workflow.h>
#include <workflow/WFTaskFactory.h>
#include <workflow/WFFacilities.h>
#include <spdlog/spdlog.h>

using namespace protocol;

void http_callback(WFHttpTask *task)
{
    HttpRequest *req = task->get_req();
    HttpResponse *resp = task->get_resp();

    spdlog::info("req uri : {}, resp status : {}",
                 req->get_request_uri(), resp->get_status_code());
}

WFHttpTask *create_http_task(const std::string &url)
{
    const int k_redirect_max = 4;
    const int k_retry_max = 2;
    return WFTaskFactory::create_http_task(url, k_redirect_max, k_retry_max, http_callback);
}

int main()
{
    WFHttpTask *first_task = create_http_task("http://www.baidu.com");

    WFFacilities::WaitGroup wait_group(1);
    auto series_callback = [&wait_group](const SeriesWork *series)
    {
        spdlog::info("All tasks have done");
        wait_group.done();
    };
    SeriesWork *series = Workflow::create_series_work(first_task, series_callback);
    series->push_back(create_http_task("http://www.bing.com"));
    series->push_back(create_http_task("http://www.sogo.com"));
    series->start();
    wait_group.wait();
}