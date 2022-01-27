// 测试SeriesWork的dismiss

#include <workflow/WFHttpServer.h>
#include <workflow/WFFacilities.h>
#include <signal.h>

static WFFacilities::WaitGroup wait_group(1);

void sig_handler(int signo)
{
    wait_group.done();
}

int main()
{
    signal(SIGINT, sig_handler);
    WFHttpServer server([](WFHttpTask *task) { 
        int cnt = 1;
        WFGoTask *go_task1 = WFTaskFactory::create_go_task("go", [](int n) { fprintf(stderr, "%d\n", n); }, cnt++);
        **task << go_task1;
        WFGoTask *go_task2 = WFTaskFactory::create_go_task("go", [](int n) { fprintf(stderr, "%d\n", n); }, cnt++);
        **task << go_task2;
        // series_of(task)->cancel();
        // series_of(task)->dismiss();
    });
    
    if (server.start(8888) == 0) {
        wait_group.wait();
        server.stop();
    }

    return 0;
}
