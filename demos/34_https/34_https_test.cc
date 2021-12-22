// https://stackoverflow.com/questions/24611640/curl-60-ssl-certificate-problem-unable-to-get-local-issuer-certificate
// https://zhuanlan.zhihu.com/p/86926335
// https://blog.csdn.net/weiyuanke/article/details/87256937

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
	fprintf(stderr, "uri : %s\n", req->get_request_uri());
	resp->append_output_body_nocopy("test\n", 5);
}

int main(int argc, char **argv)
{
	if (argc != 3)
	{
		fprintf(stderr, "%s [cert file] [key file]\n",
				argv[0]);
		exit(1);
	}

	WFHttpServer server(process);

	if (server.start(8888, argv[1], argv[2]) == 0)
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