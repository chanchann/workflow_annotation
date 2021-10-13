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
    // 可以创建一个空的并行，然后通过add_series接口向并行中添加串行
    // 也可以在创建时指定一组串行
    // 并行本身也是一种任务，所以并行也可以放到串行中。
    // 回调函数用于通知用户该并行中的串行均已完成，不能再继续添加新的串行，且回调函数结束后，该并行会立即被销毁。
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
    // start会自动创建一个串行，并将parallel作为first_task立即开始执行
    pwork->start();
    wait_group.wait();

    return 0;
}
