#include <signal.h>
#include <string>
#include <workflow/HttpMessage.h>
#include <workflow/HttpUtil.h>
#include <workflow/WFServer.h>
#include <workflow/WFHttpServer.h>
#include <workflow/WFFacilities.h>
#include <arpa/inet.h>

void process(WFHttpTask *server_task)
{
	protocol::HttpRequest *req = server_task->get_req();
	protocol::HttpResponse *resp = server_task->get_resp();

	resp->append_output_body("Hello World", 11);

	resp->set_http_version("HTTP/1.1");
	resp->set_status_code("200");
	resp->set_reason_phrase("OK");

	resp->add_header_pair("Content-Type", "text/html");
	resp->add_header_pair("Server", "Sogou WFHttpServer");

	/* print some log */
	struct sockaddr_storage addr;
	socklen_t sock_len = sizeof addr;
	server_task->get_peer_addr(reinterpret_cast<struct sockaddr *>(&addr), &sock_len);

	char addrstr[128];
	unsigned short port = 0;
	if (addr.ss_family == AF_INET)
	{ // ipv4
		struct sockaddr_in *sin = reinterpret_cast<struct sockaddr_in *>(&addr);
		inet_ntop(AF_INET, &sin->sin_addr, addrstr, 128);
		port = ntohs(sin->sin_port);
	}
	else if (addr.ss_family == AF_INET6)
	{ // ipv6
		struct sockaddr_in6 *sin6 = reinterpret_cast<struct sockaddr_in6 *>(&addr);
		inet_ntop(AF_INET6, &sin6->sin6_addr, addrstr, 128);
		port = ntohs(sin6->sin6_port);
	}
	else
	{
		strcpy(addrstr, "Unknown");
	}
    fprintf(stderr, "Peer address: %s : %d\n", addrstr, port);
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
	uint16_t port = 8888;

	if (server.start(port) == 0)
	{
		wait_group.wait();
		server.stop(); // 关停是非暴力式的，会等待正在服务的请求执行完。
	}
	else
	{
        fprintf(stderr, "Cannot start server");
		exit(1);
	}

	return 0;
}