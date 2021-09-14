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

// state在 /src/factory/WFTask.h中看 enum
// get_error在 /src/factory/WFTaskError.h 中查看

// state = 1, error = 11 即 state == SYS_ERROR, error = EAGAIN。说明没有连接可以用了
// 解决办法 改一下global settings，把max_connections改到1000以上。这个值默认为200
// 见client02.cc 

void http_callback(WFHttpTask *task)
{
    // state代表任务的结束状态, 在WFTask.h文件中，可以看到所有可能的状态值：
    spdlog::info("state : {}, err : {}, seq : {}",
                 task->get_state(), task->get_error(), seq++);
}

int main()
{
    std::string url = "http://127.0.0.1:8888/";

    WFFacilities::WaitGroup wait_group(1);

    ParallelWork *pwork = Workflow::create_parallel_work([&wait_group](const ParallelWork *pwork)
                                                         {
                                                             spdlog::info("All series in this parallel have done");
                                                             wait_group.done();
                                                         });

    for (int i = 0; i < 400; i++)
    {
        // 工厂类接口，都是确保成功的。也就是说，一定不会返回NULL。用户无需对返回值做检查
        // 当URL不合法时，工厂也能正常产生task。并且在任务的callback里再得到错误。
        WFHttpTask *task = WFTaskFactory::create_http_task(url,
                                                           k_redirect_max,
                                                           k_retry_max,
                                                           http_callback);
        SeriesWork *series = Workflow::create_series_work(task, nullptr);
        pwork->add_series(series);
    }

    spdlog::info("client start");
    // start会自动创建一个串行，并将parallel作为first_task立即开始执行
    pwork->start();
    wait_group.wait();

    return 0;
}
