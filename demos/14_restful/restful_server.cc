#include <iostream>
#include <workflow/Workflow.h>
#include <workflow/WFTaskFactory.h>
#include <workflow/WFFacilities.h>
#include <workflow/WFHttpServer.h>
#include <spdlog/spdlog.h>
#include <memory>
#include <signal.h>
#include <unordered_map>
#include "base64.h"
#include "encode.h"

using namespace protocol;

struct series_context {
    std::string params;
    WFHttpTask *server_task;
};

void http_callback(WFHttpTask *post_task) {
    HttpResponse *post_resp = post_task->get_resp();
    spdlog::info("Http status : {}", post_resp->get_status_code());

    const void *body;
    size_t body_len;
    post_resp->get_parsed_body(&body, &body_len);

    series_context *ctx = static_cast<series_context *>(series_of(post_task)->get_context());
    ctx->server_task->get_resp()->append_output_body_nocopy(body, body_len);

    spdlog::info("http callback done");
}

void create_http_post(WFHttpTask *server_task) {
    spdlog::info("create http post");
    HttpRequest *req = server_task->get_req();
    
    const void *body;
    size_t len;
    req->get_parsed_body(&body, &len);
    spdlog::info("img size : {}", len);

    // https://ai.baidu.com/tech/imagerecognition/redwineh
	std::string url = "https://aip.baidubce.com/rest/2.0/image-classify/v1/redwine?access_token=[token]";

    WFHttpTask *post_task = WFTaskFactory::create_http_task(url, 4, 2, http_callback);
    
    series_context* ctx = new series_context;

    std::string data = "image=" + url_encode(base64_encode(static_cast<unsigned char const*>(body), len));

    ctx->params = std::move(data);
    ctx->server_task = server_task;

    series_of(server_task)->set_context(ctx);
    HttpRequest *post_req = post_task->get_req();

    post_req->set_method("POST");
    post_req->add_header_pair("Host", "aip.baidubce.com");
    post_req->add_header_pair("Content-Type", "application/x-www-form-urlencoded");
    post_req->append_output_body_nocopy(
                                static_cast<const void *>(ctx->params.c_str()),
                                ctx->params.size());
    **server_task << post_task;
}

void process(WFHttpTask *server_task) {
    if (strcmp(server_task->get_req()->get_request_uri(), "/api") == 0)
    {
        create_http_post(server_task);
        server_task->set_callback([](WFHttpTask *server_task) {
            spdlog::info("delete ctx");
            delete static_cast<series_context *>(series_of(server_task)->get_context());
        });
        return;
    }
    else
    {
        server_task->get_resp()->append_output_body("<html>server test</html>");
        return;
    }
}

static WFFacilities::WaitGroup wait_group(1);

void sig_handler(int signo)
{
    wait_group.done();
}

int main()
{
    signal(SIGINT, sig_handler);
    WFHttpServer server(process);
    
    if (server.start(8888) == 0) {
        wait_group.wait();
        server.stop();
    }

    return 0;
}
