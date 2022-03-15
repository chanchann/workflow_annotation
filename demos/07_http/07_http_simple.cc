#include <iostream>
#include <workflow/Workflow.h>
#include <workflow/WFTaskFactory.h>

using namespace protocol;

int main()
{

    WFHttpTask *task = 
        WFTaskFactory::create_http_task("http://www.baidu.com",
                                        4,
                                        2,
                                        [](WFHttpTask *task) {
                                            HttpResponse *resp = task->get_resp();
                                            fprintf(stderr, "Http status : %s\n", resp->get_status_code());
                                        });

    task->start();
    getchar();
}
