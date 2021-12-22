#include <workflow/WFHttpServer.h>

void Factorial(int n, protocol::HttpResponse *resp)
{
    unsigned long long factorial = 1;
    for(int i = 1; i <= n; i++)
    {
        factorial *= i;
    }
    resp->append_output_body(std::to_string(factorial));
}

void process(WFHttpTask *server_task)
{
    const char *uri = server_task->get_req()->get_request_uri();
    if (*uri == '/')
        uri++;
    int n = atoi(uri);
    protocol::HttpResponse *resp = server_task->get_resp();
    WFGoTask *go_task = WFTaskFactory::create_go_task("go", Factorial, n, resp);
    **server_task << go_task;
}

int main()
{
    WFHttpServer server(process);

    if (server.start(8888) == 0)
    {
        getchar();
        server.stop();
    }
    return 0;
}