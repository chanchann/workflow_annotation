#include <workflow/UpstreamManager.h>
#include <workflow/WFHttpServer.h>
#include <workflow/WFTaskFactory.h>
#include <workflow/WFFacilities.h>
#include <workflow/UpstreamPolicies.h>
#include <vector>
#include <string>
#include <memory>
#include <signal.h>
#include "util.h"

static WFFacilities::WaitGroup wait_group(1);
const int server_num = 3;

void sig_handler(int signo)
{
    wait_group.done();
}

void server_process(WFHttpTask *task, std::string& name)
{
    auto *resp = task->get_resp();
    fprintf(stderr, "name : %s\n", name.c_str());
    resp->append_output_body(name);
}

int main()
{
    signal(SIGINT, sig_handler);
    std::vector<std::unique_ptr<WFHttpServer> > server_list(server_num);
    unsigned int port = 8000;
    for (int i = 0; i < server_num; i++)
    {
        server_list[i] = 
            util::make_unique<WFHttpServer>(std::bind(&server_process, 
                                            std::placeholders::_1, 
                                            "server-" + std::to_string(i)));

        if (server_list[i]->start("127.0.0.1", port++) != 0)
        {
            fprintf(stderr, "http server start failed");
            wait_group.done();
            break;
        } 
    }

    wait_group.wait();
    for(auto& server : server_list) {
        server->stop();
    }
    return 0;
}