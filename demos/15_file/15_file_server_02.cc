/*
我们现在文件任务已经支持文件名接口了，你也可以用文件名来产生任务，无需维护fd。

内部每次任务都打开和关闭fd。

WFFileIOTask *create_pwrite_task(const std::string& pathname, const void *buf, size_t count, off_t offset,
                                                           fio_callback_t callback);
*/

#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <utility>
#include <string>
#include <workflow/HttpMessage.h>
#include <workflow/WFHttpServer.h>
#include <workflow/WFTaskFactory.h>
#include <workflow/Workflow.h>
#include <workflow/WFFacilities.h>
#include <sys/stat.h>

using namespace protocol;

void pread_callback(WFFileIOTask *task)
{
    /*
    struct FileIOArgs
    {
        int fd;
        void *buf;
        size_t count;
        off_t offset;
    };
    */
    FileIOArgs *args = task->get_args();
    // 当ret < 0, 任务错误。否则ret为读取到数据的大小
    // 在文件任务里，ret < 0与task->get_state() != WFT_STATE_SUCCESS完全等价。
    long ret = task->get_retval();
    // server_task 的resp
    HttpResponse *resp = static_cast<HttpResponse *>(task->user_data);

    if (task->get_state() != WFT_STATE_SUCCESS || ret < 0)
    {
        resp->set_status_code("503");
        resp->append_output_body("<html>503 Internal Server Error.</html>");
    }
    else /* Use '_nocopy' carefully. */
        // buf域的内存我们是自己管理的，可以通过append_output_body_nocopy()传给resp。
        resp->append_output_body_nocopy(args->buf, ret);
}

void process(WFHttpTask *server_task, const std::string &root)
{
    HttpRequest *req = server_task->get_req();
    HttpResponse *resp = server_task->get_resp();
    const char *uri = req->get_request_uri();
    fprintf(stderr, "Request-URI: %s\n", uri);

    const char *p = uri;
    while (*p && *p != '?')
        p++;

    std::string abs_path(uri, p - uri);
    abs_path = root + abs_path;
    if (abs_path.back() == '/')
        abs_path += "index.html";
    fprintf(stderr, "abs_path : %s\n", abs_path.c_str());

    resp->add_header_pair("Server", "Sogou C++ Workflow Server");

    struct stat st;
    stat(abs_path.c_str(), &st);
    size_t size = st.st_size;
    void *buf = malloc(size);
    
    // WFFileIOTask *create_pwrite_task(const std::string& pathname, const void *buf, size_t count, off_t offset,
    //                                                         fio_callback_t callback);
    WFFileIOTask *pread_task = WFTaskFactory::create_pread_task(abs_path, buf, size, 0,
                                                                pread_callback);
    /* To implement a more complicated server, please use series' context
        * instead of tasks' user_data to pass/store internal data. */
    pread_task->user_data = resp; /* pass resp pointer to pread task. */
    server_task->user_data = buf; /* to free() in callback() */
    // 在回复完成后，我们会free()这块内存
    server_task->set_callback([](WFHttpTask *t)
                                { free(t->user_data); });
    series_of(server_task)->push_back(pread_task);

}

static WFFacilities::WaitGroup wait_group(1);

void sig_handler(int signo)
{
    wait_group.done();
}

int main()
{
    signal(SIGINT, sig_handler);

    unsigned short port = 8888;
    std::string root = "."; // 服务的根路径
    WFHttpServer server(std::bind(process, std::placeholders::_1, std::ref(root)));

    if (server.start(port) == 0)
    {
        wait_group.wait();
        server.stop();
    }
    else
    {
        fprintf(stderr, "start server failed");
        exit(1);
    }

    return 0;
}