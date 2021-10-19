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

void server_process(WFHttpTask *task, std::string &name)
{
    auto *resp = task->get_resp();
    resp->add_header_pair("Content-Type", "text/plain");
    resp->append_output_body_nocopy(static_cast<const char *>(name.c_str()), name.size());
}

int main()
{
    signal(SIGINT, sig_handler);
    std::vector<std::unique_ptr<WFHttpServer>> server_list;
    unsigned int port = 8000;
    for (int i = 0; i < 3; i++)
    {
        std::string server_name = "server-" + std::to_string(i);
        auto deleter = [](WFHttpServer *server)
        {
            server->stop();
        }; 
        server_list[i] = 
            std::unique_ptr<WFHttpServer, decltype(deleter)>(new WFHttpServer(std::bind(&server_process, 
                                                            std::placeholders::_1, server_name)), 
                                                            deleter);
        if (server_list[i]->start("127.0.0.1", port++) != 0)
        {
            fprintf(stderr, "http server start failed");
            wait_group.done();
            break;
        }
    }

    wait_group.wait();
    return 0;
}