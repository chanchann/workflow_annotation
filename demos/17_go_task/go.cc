
// 计算任务：go任务
/*
计算任务的调度规则是什么
我们发现包括WFGoTask在内的所有计算任务，在创建时都需要指定一个计算队列名，这个计算队列名可用于指导我们内部的调度策略。
首先，只要有空闲计算线程可用，任务将实时调起，计算队列名不起作用。
当计算线程无法实时调起每个任务的时候，那么同一队列名下的任务将按FIFO的顺序被调起，而队列与队列之间则是平等对待。
例如，先连续启动n个队列名为A的任务，再连续启动n个队列名为B的任务。
那么无论每个任务的cpu耗时分别是多少，也无论计算线程数多少，这两个队列将近倾向于同时执行完毕。
这个规律可以扩展到任意队列数量以及任意启动顺序。
*/

#include <workflow/WFHttpServer.h>
#include <workflow/WFFacilities.h>
#include <signal.h>

void Fibonacci(int n, protocol::HttpResponse *resp)
{
    unsigned long long x = 0, y = 1;
    char buf[256];
    int i;

    if (n <= 0 || n > 94)
    {
        resp->append_output_body_nocopy("<html>Invalid Number.</html>",
                                        strlen("<html>Invalid Number.</html>"));
        return;
    }

    resp->append_output_body_nocopy("<html>", strlen("<html>"));
    for (i = 2; i < n; i++)
    {
        sprintf(buf, "<p>%llu + %llu = %llu.</p>", x, y, x + y);
        resp->append_output_body(buf);
        y = x + y;
        x = y - x;
    }

    if (n == 1)
        y = 0;
    sprintf(buf, "<p>The No. %d Fibonacci number is: %llu.</p>", n, y);
    resp->append_output_body(buf);
    resp->append_output_body_nocopy("</html>", strlen("</html>"));
}

void process(WFHttpTask *task)
{
    const char *uri = task->get_req()->get_request_uri();
    if (*uri == '/')
        uri++;

    int n = atoi(uri);
    protocol::HttpResponse *resp = task->get_resp();
    // "go" 代表计算任务所在队列名。一般来讲同一种计算任务用一个队列名就可以
    // go task的callback默认为空，但用户同样可以通过set_callback()接口给go task设置一个callback。
    WFGoTask *go_task = WFTaskFactory::create_go_task("go", Fibonacci, n, resp);
    go_task->set_callback([](WFGoTask *task)
                          { fprintf(stderr, "go task done"); });
    // 默认情况下，框架产生一个与主机CPU数相同的计算线程用于执行计算任务
    series_of(task)->push_back(go_task);
    // 如果我们只使用go task，整个Workflow框架退化成一个线程池。
    // 但是这个线程池比一般的线程池又有更多的功能，比如每个任务有queue name，任务之间还可以组成各种串并联或更复杂的依赖关系。
    // 而通过计算队列名与series，我们又实现了线程任务的调度控制与任务之间的依赖。
}

static WFFacilities::WaitGroup wait_group(1);

void sig_handler(int signo)
{
    wait_group.done();
}

int main()
{
    signal(SIGINT, sig_handler);
    WFHttpServer server(process);

    if (server.start(8888) == 0)
    {
        wait_group.wait();
        server.stop();
    }

    return 0;
}