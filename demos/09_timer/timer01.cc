#include <chrono>
#include <workflow/Workflow.h>
#include <workflow/WFTaskFactory.h>
#include <spdlog/spdlog.h>

// todo : not finished

// main函数结束或exit()被调用的时候，所有任务必须里运行到callback，并且没有新的任务被调起。
// 这时就可能出现一个问题，定时器的定时周期可以非常长，并且不能主动打断（打断定时器的功能正在研发）。如果等定时器到期，程序退出需要很长时间。
// 而实现上，程序退出是可以打断定时器，让定时器回到callback的。如果定时器被程序退出打断，get_state()会得到一个WFT_STATE_ABORTED状态。
// 当然如果定时器被程序退出打断，则不能再调起新的任务。

bool program_terminate = false;
std::mutex mutex;

void timer_callback(WFTimerTask *timer)
{
    mutex.lock();
    // 必须在锁里判断program_terminate条件，否则可能在程序已经结束的情况下又调起新任务。
    if (!program_terminate)
    {
        WFHttpTask *task;
        if (urls_to_fetch > 0)
        {
            task = WFTaskFactory::create_http_task(...);
            series_of(timer)->push_back(task);
        }

        series_of(timer)->push_back(WFTaskFactory::create_timer_task(1, 0, timer_callback));
    }
    mutex.unlock();
}

...
int main()
{
    ....
    /* all urls done */
    mutex.lock();
    program_terminate = true;
    mutex.unlock();
    return 0;
}
