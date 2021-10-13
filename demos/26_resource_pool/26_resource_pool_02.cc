#include <workflow/Workflow.h>
#include <workflow/WFTaskFactory.h>
#include <workflow/WFFacilities.h>
#include <vector>

using namespace protocol;

void http_callback(WFHttpTask *task)
{
    HttpRequest *req = task->get_req();
    HttpResponse *resp = task->get_resp();
    fprintf(stderr, "req uri : %s, resp status : %s",
            req->get_request_uri(), resp->get_status_code());
}

int main()
{
    const int MAX_PARALLEL = 1;   // 设置为1，那么则就变成了串行了
    WFResourcePool pool(MAX_PARALLEL);
    std::vector<std::string> urls = {
        "http://www.baidu.com",
        "http://www.bing.com",
        "http://www.sogou.com"
    };

    WFFacilities::WaitGroup wait_group(3);

    for (auto &url : urls)
    {
        WFHttpTask *task = WFTaskFactory::create_http_task(url, 4, 2,
        [&pool, &wait_group, &url](WFHttpTask *task)
        {
            fprintf(stderr, "pool post : url[%s]", url.c_str());
            pool.post(nullptr); 
            wait_group.done();
        });
        // 新版代码中无需保存res，可以不传resbuf参数。
        // 多了一个这个接口 : WFConditional *get(SubTask *task);
        // 我们旧版这个代码没有这个，所以随便拿个来装一下
        WFConditional *cond = pool.get(task, &task->user_data); 
        cond->start();
    }
    wait_group.wait();

    return 0;
}
