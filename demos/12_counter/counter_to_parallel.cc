// 用匿名计数器实现任务并行
// 计数器的存在，可以让我们构建更复杂的任务依赖关系，比如实现一个全连接的神经网络
// 计数器在我们框架里，往往用于实现异步锁，或者用于任务之间的通道。形态上更像一种控制任务。

#include <spdlog/spdlog.h>
#include <workflow/WFTaskFactory.h>
#include <workflow/WFHttpServer.h>
#include <workflow/WFFacilities.h>
#include <mutex>
#include <condition_variable>
#include <signal.h>

using namespace protocol;

void http_callback(WFHttpTask *task)
{
    HttpRequest *req = task->get_req();
    HttpResponse *resp = task->get_resp();
    spdlog::info("req uri : {}, resp status : {}",
                 req->get_request_uri(), resp->get_status_code());

    WFCounterTask *counter = static_cast<WFCounterTask *>(task->user_data);
    counter->count(); // 每个http任务完成之后，调用一次count。
    // 注意，匿名计数器的count次数不可以超过目标值，否则counter可能已经callback销毁了，程序行为无定义。
    // 而count_by_name("c1") 不会有匿名计数器野指针的问题
}

std::mutex mutex;
std::condition_variable cond;
bool finished = false;

void counter_callback(WFCounterTask *counter)
{
    std::lock_guard<std::mutex> lock(mutex);
    finished = true;
    cond.notify_one();
}

int main(int argc, char *argv[])
{
    std::vector<std::string> urls = {
        "http://www.baidu.com",
        "http://www.bing.com",
        "http://www.sogo.com"};
    size_t url_count = urls.size();
    // 创建一个目标值为url_count的计数器，每个http任务完成之后，调用一次count
    WFCounterTask *counter = WFTaskFactory::create_counter_task(url_count, counter_callback);

    for (size_t i = 0; i < url_count; i++)
    {
        auto task = WFTaskFactory::create_http_task(urls[i], 4, 2, http_callback);
        // user_data属于task，所以它使用的时期是task开始前和它的callback里；
        task->user_data = counter;
        task->start();
    }
    counter->start();
    // counter->start()调用可以放在for循环之前。counter只要被创建，就可以调用其count接口，无论counter是否已经启动。
    {
        std::unique_lock<std::mutex> lock(mutex);
        cond.wait(lock, []{ return finished; });
    }
    return 0;
}