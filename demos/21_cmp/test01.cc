#include <workflow/Workflow.h>
#include <workflow/WFTaskFactory.h>
#include <workflow/WFFacilities.h>
#include <atomic>
#include <stdio.h>

using namespace protocol;

std::atomic<int> seq{0};
void http_callback(WFHttpTask *task)
{
    HttpRequest *req = task->get_req();
    req->add_header_pair("Connection", "Keep-Alive");
    fprintf(stderr, "seq : %d\n", seq++);
}

WFHttpTask *create_http_task(const std::string &url)
{
    return WFTaskFactory::create_http_task(url, 4, 2, http_callback);
}

int main()
{
    WFHttpTask *first_task = create_http_task("http://www.baidu.com");

    WFFacilities::WaitGroup wait_group(1);
    auto series_callback = [&wait_group](const SeriesWork *series)
    {
        fprintf(stderr, "all one\n");
        wait_group.done();
    };
    SeriesWork *series = Workflow::create_series_work(first_task, series_callback);
    for(int i = 0; i < 20; i++) {
        series->push_back(create_http_task("http://www.bing.com"));
    }
    series->start();
    wait_group.wait();
}