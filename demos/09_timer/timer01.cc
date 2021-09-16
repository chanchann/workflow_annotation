#include <workflow/WFTaskFactory.h>
#include <workflow/WFFacilities.h>
#include <spdlog/spdlog.h>

static WFFacilities::WaitGroup wait_group(1);

void callback(WFHttpTask *task)
{
    const void *body;
    size_t size;

    if (task->get_resp()->get_parsed_body(&body, &size))
        fwrite(body, 1, size, stdout);

    WFTimerTask *timer = WFTaskFactory::create_timer_task(1000 * 1000, [](WFTimerTask *timer)
                                                          {
                                                              spdlog::info("Timer ends.");
                                                              wait_group.done();
                                                          });
    series_of(task)->push_back(timer);
    spdlog::info("Wait for 1 second...");  // timer不阻塞，先执行
}

int main(void)
{
    WFHttpTask *task = WFTaskFactory::create_http_task("http://www.baidu.com/", 3, 2, callback);
    task->start();
    wait_group.wait();
    return 0;
}