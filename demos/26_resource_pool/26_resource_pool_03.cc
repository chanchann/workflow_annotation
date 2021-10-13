#include <workflow/Workflow.h>
#include <workflow/WFTaskFactory.h>
#include <workflow/WFFacilities.h>
#include <vector>
#include <signal.h>

using namespace protocol;

static WFFacilities::WaitGroup wait_group(1);

void sig_handler(int signo)
{
    wait_group.done();
}

int main()
{
    signal(SIGINT, sig_handler);
    WFResourcePool pool(1);
    WFHttpTask *task1 =
        WFTaskFactory::create_http_task("http://www.baidu.com", 4, 2,
        [&pool](WFHttpTask *task)
        {
            // 当用户对资源使用完毕（一般在任务callback里），需要通过post()接口把资源放回池子。
            fprintf(stderr, "task 1 resp status : %s\n",
                    task->get_resp()->get_status_code());
            pool.post(nullptr);
        });
    WFHttpTask *task2 =
        WFTaskFactory::create_http_task("http://www.bing.com", 4, 2,
        [&pool](WFHttpTask *task)
        {
            fprintf(stderr, "task 2 resp status : %s\n",
                    task->get_resp()->get_status_code());
            pool.post(nullptr);
        });

    // get()接口，把任务打包成一个conditional。conditional是一个条件任务，条件满足时运行其包装的任务。
    // get()接口可包含第二个参数是一个void **resbuf，用于保存所获得的资源。
    // 新版代码接口中这里无需第二个参数
    // WFConditional *cond_task_1 = pool.get(task1); 
    // WFConditional *cond_task_2 = pool.get(task2);

    // 注意conditional是在它被执行时去尝试获得资源的，而不是在它被创建的时候
    WFConditional *cond_task_1 = pool.get(task1, &task1->user_data); 
    WFConditional *cond_task_2 = pool.get(task2, &task2->user_data);

    // cond_task_1先创建，等待task2结束后才运行
    // 这里并不会出现cond_task_2卡死，因为conditional是在执行时才获得资源的。
    cond_task_2->start();
    // wait for t2 finish here.
    fprintf(stderr, "pile here\n");  // 因为是异步的，所以先出现，不要被wait for t2 finish here.迷惑了
    cond_task_1->start();
    wait_group.wait();

    return 0;
}
