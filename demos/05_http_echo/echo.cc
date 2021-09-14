#include <signal.h>

#include <string>
#include <workflow/HttpMessage.h>
#include <workflow/HttpUtil.h>
#include <workflow/WFServer.h>
#include <workflow/WFHttpServer.h>
#include <workflow/WFFacilities.h>
#include <spdlog/spdlog.h>

#include <arpa/inet.h>

// 因为返回的页面很小，我们没有关注回复成功与否。http_proxy demo中我们将看到如果获得回复的状态。
void process(WFHttpTask *server_task)
{
	protocol::HttpRequest *req = server_task->get_req();
	protocol::HttpResponse *resp = server_task->get_resp();

	/* Set response message body. */
	resp->append_output_body_nocopy("<html>", 6);

	char buf[8192];
	int len = snprintf(buf, 8192, "<p>%s %s %s</p>", req->get_method(),
					   req->get_request_uri(), req->get_http_version());
	resp->append_output_body(buf, len);
	protocol::HttpHeaderCursor cursor(req);
	std::string name;
	std::string value;
	while (cursor.next(name, value))
	{
		len = snprintf(buf, 8192, "<p>%s: %s</p>", name.c_str(), value.c_str());
		resp->append_output_body(buf, len);
		// append_output_body()。显然让用户生成完整的http body再传给我们并不太高效。
		// 用户只需要调用append接口，把离散的数据一块块扩展到message里就可以了。
		// append_output_body()操作会把数据复制走
	}
	// _nocopy后缀的接口会直接引用指针，使用时需要注意不可以指向局部变量。
	// append_output_body_nocopy()接口时，buf指向的数据的生命周期至少需要延续到task的callback。
	resp->append_output_body_nocopy("</html>", 7);

	/* Set status line if you like. */
	resp->set_http_version("HTTP/1.1");
	resp->set_status_code("200");
	resp->set_reason_phrase("OK");

	resp->add_header_pair("Content-Type", "text/html");
	resp->add_header_pair("Server", "Sogou WFHttpServer");

	long long seq = server_task->get_task_seq();
	if (seq == 9) /* no more than 10 requests on the same connection. */
		// 关闭连接还可以通过task->set_keep_alive()接口来完成，但对于http协议，还是推荐使用设置header的方式。
		resp->add_header_pair("Connection", "close");

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
	spdlog::info("Peer address: {} : {}, seq: {}", addrstr, port, seq);
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
		spdlog::critical("Cannot start server");
		exit(1);
	}

	return 0;
}