// 让server延迟3s返回内容

// server的回复时机，并不是process函数的结束，而是在server任务所在的series已经没有任务的时候。
// 那么，我们只需要简单的在这个series里加入一个定时器，定时器将在process()函数之后被拉起，
// 并且在定时结束之后回复请求（series已空）。

#include <workflow/WFTaskFactory.h>
#include <workflow/WFHttpServer.h>
#include <workflow/WFFacilities.h>

static WFFacilities::WaitGroup wait_group(1);

int main()
{
    WFHttpServer server([](WFHttpTask *task) {

        task->get_resp()->append_output_body("<html>Hello World!</html>");
        series_of(task)->push_back(WFTaskFactory::create_timer_task(1000 * 1000,
                                                                    [](WFTimerTask *timer)
                                                                    { 
                                                                        wait_group.done(); 
                                                                    }));
    });

    if (server.start(8888) == 0)
    {
        wait_group.wait();
        server.stop();
    }

    return 0;
}