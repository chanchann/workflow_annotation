// 模拟 https://github.com/sogou/workflow/issues/135

#include <workflow/Workflow.h>
#include <workflow/WFTaskFactory.h>
#include <workflow/WFFacilities.h>
#include <spdlog/spdlog.h>
#include <vector>
#include <atomic>

using namespace protocol;

const int k_redirect_max = 4;
const int k_retry_max = 2;

std::atomic<int> seq;

void http_callback(WFHttpTask *task)
{
    spdlog::info("state : {}, err : {}, seq : {}",
                 task->get_state(), task->get_error(), seq++);
}

int main()
{
    std::string url = "http://127.0.0.1:8888/";

    struct WFGlobalSettings settings = GLOBAL_SETTINGS_DEFAULT;
    settings.endpoint_params.max_connections = 1024;
    WORKFLOW_library_init(&settings);

    WFFacilities::WaitGroup wait_group(1);

    ParallelWork *pwork = Workflow::create_parallel_work([&wait_group](const ParallelWork *pwork)
                                                         {
                                                             spdlog::info("All series in this parallel have done");
                                                             wait_group.done();
                                                         });

    for (int i = 0; i < 400; i++)
    {
        WFHttpTask *task = WFTaskFactory::create_http_task(url,
                                                           k_redirect_max,
                                                           k_retry_max,
                                                           http_callback);
        SeriesWork *series = Workflow::create_series_work(task, nullptr);
        pwork->add_series(series);
    }

    spdlog::info("client start");
    pwork->start();
    wait_group.wait();

    return 0;
}
