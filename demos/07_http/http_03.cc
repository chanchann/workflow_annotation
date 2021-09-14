// 同时发起多个http请求

#include <workflow/Workflow.h>
#include <workflow/WFTaskFactory.h>
#include <workflow/WFFacilities.h>
#include <spdlog/spdlog.h>
#include <vector>

using namespace protocol;

const int k_redirect_max = 4;
const int k_retry_max = 2;

void http_callback(WFHttpTask *task)
{
    HttpRequest *req = task->get_req();
    HttpResponse *resp = task->get_resp();
    spdlog::info("req uri : {}, resp status : {}",
                 req->get_request_uri(), resp->get_status_code());
}

int main()
{
    std::vector<std::string> urls = {
        "http://www.baidu.com",
        "http://www.bing.com",
        "http://www.sogo.com"};

    WFFacilities::WaitGroup wait_group(1);
    ParallelWork *pwork = Workflow::create_parallel_work([&wait_group](const ParallelWork *pwork)
                                                         {
                                                             spdlog::info("All series in this parallel have done");
                                                             wait_group.done();
                                                         });

    for (auto &url : urls)
    {
        WFHttpTask *task = WFTaskFactory::create_http_task(url,
                                                           k_redirect_max,
                                                           k_retry_max,
                                                           http_callback);
        SeriesWork *series = Workflow::create_series_work(task, nullptr);
        pwork->add_series(series);
    }

    pwork->start();
    wait_group.wait();

    return 0;
}
