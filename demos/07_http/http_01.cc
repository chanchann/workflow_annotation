// 发起一个http请求

#include <iostream>
#include <workflow/Workflow.h>
#include <workflow/WFTaskFactory.h>
#include <workflow/WFFacilities.h>
#include <spdlog/spdlog.h>
#include <signal.h>

using namespace protocol;

#define redirect_max 4
#define retry_max 2

void http_callback(WFHttpTask *task) {
    HttpResponse *resp = task->get_resp();
    spdlog::info("Http status : {}", resp->get_status_code());
    
    // response body 
    const void* body;
    size_t body_len;
    resp->get_parsed_body(&body, &body_len);
    
    	// write body to file
	FILE *fp = fopen("res.txt", "w");
	fwrite(body, 1, body_len, fp);
	fclose(fp);

    spdlog::info("write file done");
}

static WFFacilities::WaitGroup wait_group(1);

void sig_handler(int signo) {
    wait_group.done();
}

int main() {
    signal(SIGINT, sig_handler);
    std::string url = "http://www.baidu.com";
    WFHttpTask *task = WFTaskFactory::create_http_task(url,
                                                    redirect_max,
                                                    retry_max,
                                                    http_callback);
    task->start();
    wait_group.wait();
}