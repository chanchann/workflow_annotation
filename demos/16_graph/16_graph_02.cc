#include <workflow/WFHttpServer.h>
#include <workflow/WFFacilities.h>
#include <signal.h>

using namespace protocol;

// https://github.com/sogou/workflow/issues/607

static WFFacilities::WaitGroup wait_group(1);

void sig_handler(int signo)
{
    wait_group.done();
}

void http_callback(WFHttpTask *http_task)
{
    HttpResponse *resp = http_task->get_resp();
    
    const void *body;
    size_t body_len;
    resp->get_parsed_body(&body, &body_len);
    fprintf(stderr, "body len : %zu\n", body_len);
}

int main()
{
    signal(SIGINT, sig_handler);
    WFHttpServer server([](WFHttpTask *server_task) { 
        WFGraphTask *graph = WFTaskFactory::create_graph_task([](const WFGraphTask *){
            fprintf(stderr, "graph callback");
        });
        
        auto http_task_01 = WFTaskFactory::create_http_task("http://www.baidu.com", 4, 2, http_callback);
        auto http_task_02 = WFTaskFactory::create_http_task("http://www.bing.com", 4, 2, http_callback);
        WFGraphNode& http_node_01 = graph->create_graph_node(http_task_01);
        WFGraphNode& http_node_02 = graph->create_graph_node(http_task_02);
        // http_node_01-->http_node_02;    // 没有这句话就是没边 --> 并行
        
        **server_task << graph;
    });
    
    if (server.start(8888) == 0) {
        wait_group.wait();
        server.stop();
    }

    return 0;
}
