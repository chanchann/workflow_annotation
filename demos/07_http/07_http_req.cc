// 发起一个http请求

#include <iostream>
#include <workflow/Workflow.h>
#include <workflow/WFTaskFactory.h>
#include <workflow/WFFacilities.h>
#include <signal.h>

using namespace protocol;

#define REDIRECT_MAX 4
#define RETRY_MAX 2

void http_callback(WFHttpTask *task)
{
    HttpResponse *resp = task->get_resp();
    fprintf(stderr, "Http status : %s\n", resp->get_status_code());

    // response body
    const void *body;
    size_t body_len;
    resp->get_parsed_body(&body, &body_len);

    // write body to file
    FILE *fp = fopen("res.txt", "w");
    fwrite(body, 1, body_len, fp);
    fclose(fp);

    fprintf(stderr, "write file done");
}

static WFFacilities::WaitGroup wait_group(1);

void sig_handler(int signo)
{
    wait_group.done();
}

int main()
{
    signal(SIGINT, sig_handler);
    // logger_initConsoleLogger(stderr);
	// logger_setLevel(LogLevel_TRACE);
    std::string url = "http://www.baidu.com";
    // 通过create_xxx_task创建的对象为任务，一旦创建，必须被启动或取消
    // 工厂函数创建的对象的生命周期均由内部管理
    WFHttpTask *task = WFTaskFactory::create_http_task(url,
                                                       REDIRECT_MAX,
                                                       RETRY_MAX,
                                                       http_callback);
    // 通过start,自行以task为first_task创建一个串行并理解启动任务
    // 任务start后，http_callback回调前，用户不能再操作该任务
    // 在一个task被直接或间接 dismiss/start 之后，用户不再拥有其所有权
    // 此后用户只能在该task的回调函数内部进行操作
    task->start();
    // 当http_callback任务结束后，任务立即被释放
    wait_group.wait();
}
