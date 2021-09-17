// 计数器是我们框架中一种非常重要的基础任务，计数器本质上是一个不占线程的信号量。
// 计数器主要用于工作流的控制，包括匿名计数器和命名计数器两种，可以实现非常复杂的业务逻辑。

// 每个计数器都包含一个target_value，当计数器的计数到达target_value，callback被调用。
// 匿名计数器直接通过WFCounterTask的count方法来增加计数
// 命名计数器，可以通过count, count_by_name函数来增加计数


#include <spdlog/spdlog.h>
#include <workflow/WFTaskFactory.h>
#include <workflow/WFHttpServer.h>
#include <workflow/WFFacilities.h>
#include <signal.h>

int main()
{
    const int cnt = 3;
    WFFacilities::WaitGroup wait_group(1);
    WFCounterTask *counter =
        WFTaskFactory::create_counter_task("counter",
                                           cnt,
                                           [&wait_group](WFCounterTask *counter)
                                           {
                                               spdlog::info("counter callback");
                                               wait_group.done();
                                           });

    counter->count();
    counter->start();
    spdlog::info("counter start");
    WFTaskFactory::count_by_name("counter");
    WFTaskFactory::count_by_name("counter", 1);
    wait_group.wait();
}