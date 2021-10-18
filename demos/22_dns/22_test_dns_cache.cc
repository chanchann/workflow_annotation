
#include <workflow/Workflow.h>
#include <workflow/WFTaskFactory.h>
#include <workflow/WFFacilities.h>


// 通过本例分析
// https://github.com/sogou/workflow/commit/1736d64d91c8b54c533a6e949354bee5566d6e2c

using namespace protocol;

void http_callback(WFHttpTask *task)
{
    HttpRequest *req = task->get_req();
    HttpResponse *resp = task->get_resp();
    fprintf(stderr, "req uri : %s, resp status : %s\n",
                 req->get_request_uri(), resp->get_status_code());
}

WFHttpTask *create_http_task(const std::string &url)
{
    return WFTaskFactory::create_http_task(url, 4, 2, http_callback);
}

int main()
{
    std::vector<std::string> url_list = {
        "http://www.bing.com",
        "http://www.tencent.com",
        "http://www.sogou.com",
        "http://www.baidu.com", 
    };

    WFHttpTask *first_task = create_http_task("http://www.baidu.com");

    WFFacilities::WaitGroup wait_group(1);
    auto series_callback = [&wait_group](const SeriesWork *series)
    {
        fprintf(stderr, "All tasks have done");
        wait_group.done();
    };
    SeriesWork *series = Workflow::create_series_work(first_task, series_callback);
    for(int i = 0; i < 5; i++) 
    {
        for(auto& url : url_list) 
        {
            series->push_back(create_http_task(url));
        }
    }   
    series->start();
    wait_group.wait();
}