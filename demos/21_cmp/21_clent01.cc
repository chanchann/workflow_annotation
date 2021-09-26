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
    HttpResponse *resp = task->get_resp();
    req->add_header_pair("Connection", "Keep-Alive");
    const void* body;
    size_t body_len;
    resp->get_parsed_body(&body, &body_len);
    fprintf(stderr, "seq : %d, body : %s\n", seq++, static_cast<const char *>(body));
}

WFHttpTask *create_http_task(const std::string &url)
{
    return WFTaskFactory::create_http_task(url, 4, 2, http_callback);
}

int main()
{
    WFHttpTask *first_task = create_http_task("http://127.0.0.1:8888");

    WFFacilities::WaitGroup wait_group(1);
    auto series_callback = [&wait_group](const SeriesWork *series)
    {
        fprintf(stderr, "all one\n");
        wait_group.done();
    };
    SeriesWork *series = Workflow::create_series_work(first_task, series_callback);
    for(int i = 0; i < 100000; i++) {
        series->push_back(create_http_task("http://127.0.0.1:8888"));
    }
    series->start();
    wait_group.wait();
}