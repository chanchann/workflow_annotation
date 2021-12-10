#include <signal.h>

#include <string>
#include <workflow/HttpMessage.h>
#include <workflow/HttpUtil.h>
#include <workflow/WFServer.h>
#include <workflow/WFHttpServer.h>
#include <arpa/inet.h>

void process(WFHttpTask *server_task)
{
	protocol::HttpRequest *req = server_task->get_req();
	protocol::HttpResponse *resp = server_task->get_resp();

	// {'set': '234', 'add': '123, 234'
	// set会覆盖
    resp->set_header_pair("set", "123");
    resp->set_header_pair("set", "234");

	// add是在后面加上
    resp->add_header_pair("add", "123");
    resp->add_header_pair("add", "234");

	resp->append_output_body_nocopy("test", 4);
}

int main()
{
	WFHttpServer server(process);

	if (server.start(8888) == 0)
	{
		getchar();
		server.stop(); 
	}
	else
	{
        fprintf(stderr, "Cannot start server");
		exit(1);
	}

	return 0;
}