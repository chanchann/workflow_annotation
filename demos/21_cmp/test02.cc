#include <iostream>
#include <workflow/Workflow.h>
#include <workflow/WFTaskFactory.h>
#include <workflow/WFFacilities.h>
#include <signal.h>
#include <stdio.h>

using namespace protocol;

#define REDIRECT_MAX 4
#define RETRY_MAX 2

void http_callback(WFHttpTask *task)
{
    HttpRequest *req = task->get_req();
    HttpResponse *resp = task->get_resp();
    // cmp here 
    // req->add_header_pair("Connection", "Keep-Alive");

    const void *body;
    size_t body_len;
    resp->get_parsed_body(&body, &body_len);

    std::cout << "end" << std::endl;
}

static WFFacilities::WaitGroup wait_group(1);

void sig_handler(int signo)
{
    wait_group.done();
}

int main()
{
    signal(SIGINT, sig_handler);
    std::string url = "http://www.baidu.com";
    WFHttpTask *task = WFTaskFactory::create_http_task(url,
                                                       REDIRECT_MAX,
                                                       RETRY_MAX,
                                                       http_callback);
    task->start();
    wait_group.wait();
}
