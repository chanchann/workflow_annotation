#include <workflow/Workflow.h>
#include <workflow/WFTaskFactory.h>
#include <workflow/WFFacilities.h>
#include <signal.h>
#include "json.hpp"

using namespace protocol;
using json = nlohmann::json;

// 观察结果可看出是顺序发送，并且顺序收到的

static WFFacilities::WaitGroup wait_group(1);

void sig_handler(int signo)
{
    wait_group.done();
}

void http_callback(WFHttpTask *task)
{
    HttpResponse *resp = task->get_resp();
    const void *body;
    size_t len;
    resp->get_parsed_body(&body, &len);
    fprintf(stderr, "%s", static_cast<const char *>(body));
}

WFHttpTask *create_post_task(const std::string &url, int uid, int cnt)
{
    auto post_task = WFTaskFactory::create_http_task(url, 4, 2, http_callback);
    post_task->get_req()->set_method("POST");
    json js;
    js["uid"] = uid;
    js["cnt"] = cnt;
    post_task->get_req()->append_output_body(js.dump());
    return post_task;
}

int main()
{
    signal(SIGINT, sig_handler);
    std::string url = "http://127.0.0.1:8888/";
    const int uid = 888;   // 模拟单个uid顺序
    int cnt = 0;
    WFHttpTask *first_task = create_post_task(url, uid, cnt++);

    SeriesWork *series = Workflow::create_series_work(first_task,
    [](const SeriesWork *series)
    {
        fprintf(stderr, "All tasks have done");
    });

    for (int i = 0; i < 20; i++)
    {
        series->push_back(create_post_task(url, uid, cnt++));
    }
    
    series->start();
    wait_group.wait();
}