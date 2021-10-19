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

void sig_handler(int signo)
{
    wait_group.done();
}

void server_process(WFHttpTask *task, std::string& name)
{
    auto *resp = task->get_resp();
    // resp->add_header_pair("Content-Type", "text/plain");
    resp->append_output_body_nocopy(static_cast<const void *>(name.c_str()), name.size());
}

int main()
{
    signal(SIGINT, sig_handler);
    std::vector<std::unique_ptr<WFHttpServer> > server_list(3);
    unsigned int port = 8000;
    for (int i = 0; i < 3; i++)
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
    // todo : 做实验把这个丢进unique_ptr 的 deleter如何
    for(auto& server : server_list) {
        server->stop();
    }
    return 0;
}