// https://github.com/sogou/workflow/issues/301

#include <string>
#include <mutex>
#include <workflow/Workflow.h>
#include <workflow/WFTaskFactory.h>
#include <workflow/WFHttpServer.h>
#include <workflow/WFFacilities.h>
#include <spdlog/spdlog.h>
#include <signal.h>

using namespace protocol;

const std::string k_counter_name = "counter";

class BlockSeries
{
public:
    BlockSeries(SubTask *first, const std::string &name) : counter_name_(name)
    {
        series_ = Workflow::create_series_work(first, [](const SeriesWork *series)
                                               { spdlog::info("series done"); });
        WFCounterTask *counter = WFTaskFactory::create_counter_task(counter_name_, 1, nullptr);
        series_->push_back(counter);
    }

    ~BlockSeries()
    {
        // 假设用户已经调用过start()
        WFTaskFactory::count_by_name(counter_name_);
    }

    void start()
    {
        series_->start();
    }

    void push_back(SubTask *task)
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            // 我实际添加了两个任务，为保证这两个任务中间不被打断，必须加锁
            WFCounterTask *counter = WFTaskFactory::create_counter_task(counter_name_, 1, nullptr);
            series_->push_back(task); // 这里的push_back本来就是加了锁的
            series_->push_back(counter);
        }
        // 这个counter_by_name打开的是上一个push_back添加的counter。
        WFTaskFactory::count_by_name(counter_name_);
    }

private:
    SeriesWork *series_;
    std::string counter_name_;
    std::mutex mutex_;
};

int main()
{
    // todo : 找一个合适的demo

    return 0;
}