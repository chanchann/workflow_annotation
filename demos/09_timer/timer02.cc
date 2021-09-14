#include <chrono>
#include <workflow/Workflow.h>
#include <workflow/WFTaskFactory.h>
#include <workflow/WFFacilities.h>
#include <spdlog/spdlog.h>

int main()
{
    WFFacilities::WaitGroup wait_group(1);
    auto start = std::chrono::high_resolution_clock::now();
    WFTimerTask *task = WFTaskFactory::create_timer_task(10 * 1000,    
                                                         [&start, &wait_group](WFTimerTask *timer)
                                                         {
                                                             auto end = std::chrono::high_resolution_clock::now();
                                                             std::chrono::duration<double, std::milli> tm = end - start;
                                                             spdlog::info("time consume : {}", tm.count());
                                                             wait_group.done();
                                                         });
    task->start();
    wait_group.wait();
    return 0;
}