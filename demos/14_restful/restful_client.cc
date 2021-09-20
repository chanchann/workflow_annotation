// 发送post请求

#include <iostream>
#include <workflow/Workflow.h>
#include <workflow/WFTaskFactory.h>
#include <workflow/WFFacilities.h>
#include <spdlog/spdlog.h>
#include <signal.h>

using namespace protocol;

struct series_context
{
    std::string url;
};

void http_callback(WFHttpTask *http_task)
{
    HttpResponse *resp = http_task->get_resp();
    spdlog::info("Http status : {}", resp->get_status_code());

    // response body
    const void *body;
    size_t body_len;
    resp->get_parsed_body(&body, &body_len);
    free(http_task->user_data);
    spdlog::info("resp info : {}", static_cast<const char *>(body));
}

void pread_callback(WFFileIOTask *pread_task)
{
    struct FileIOArgs *args = pread_task->get_args();

    close(args->fd);
    if (pread_task->get_state() != WFT_STATE_SUCCESS)
    {
        free(args->buf);
        return;
    }
    auto ctx = static_cast<series_context *>(pread_task->user_data);

    WFHttpTask *http_task = WFTaskFactory::create_http_task(ctx->url, 4, 2, http_callback);
    http_task->get_req()->set_method("POST");
    http_task->get_req()->append_output_body_nocopy(args->buf, args->count); /* nocopy */
    http_task->user_data = args->buf;                                        /* To free. */
    **pread_task << http_task;
    delete ctx;
}

WFFileIOTask *create_file_request(const std::string &url, std::string img_path)
{
    spdlog::info("create file req");
    WFFileIOTask *pread_task;
    int fd = open(img_path.c_str(), O_RDONLY);
    if (fd >= 0)
    {
        size_t size = lseek(fd, 0, SEEK_END);
        void *buf = malloc(size);

        pread_task = WFTaskFactory::create_pread_task(fd, buf, size, 0,
                                                      pread_callback);
    }
    series_context *ctx = new series_context; 
    ctx->url = std::move(url);
    pread_task->user_data = ctx;   
    return pread_task;
}

static WFFacilities::WaitGroup wait_group(1);

void sig_handler(int signo)
{
    wait_group.done();
}

int main(int argc, char *argv[])
{
    signal(SIGINT, sig_handler);
    if (argc != 3)
    {
        spdlog::critical("USAGE: {} <http URL> <post img path data>", argv[0]);
        exit(1);
    }
    std::string url = argv[1];
    std::string img_path = argv[2];
    if (strncasecmp(argv[1], "http://", 7) != 0 &&
        strncasecmp(argv[1], "https://", 8) != 0)
    {
        url = "http://" + url;
    }

    auto task = create_file_request(url, img_path);
    task->start();

    wait_group.wait();
}
