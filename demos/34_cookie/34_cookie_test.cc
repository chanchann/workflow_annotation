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