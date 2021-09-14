
#include <netdb.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <workflow/HttpMessage.h>
#include <workflow/HttpUtil.h>
#include <workflow/WFTaskFactory.h>
#include <workflow/WFFacilities.h>
#include <spdlog/spdlog.h>

#define REDIRECT_MAX 5
#define RETRY_MAX 2

void wget_callback(WFHttpTask *task)
{
	protocol::HttpRequest *req = task->get_req();
	protocol::HttpResponse *resp = task->get_resp();
	int state = task->get_state();
	int error = task->get_error();

	switch (state)
	{
	case WFT_STATE_SYS_ERROR:
		spdlog::error("system error: {}", strerror(error));
		break;
	case WFT_STATE_DNS_ERROR:
		spdlog::error("DNS error: {}", gai_strerror(error));
		break;
	case WFT_STATE_SSL_ERROR:
		spdlog::error("SSL error: {}", error);
		break;
	case WFT_STATE_TASK_ERROR:
		spdlog::error("Task error: {}", error);
		break;
	case WFT_STATE_SUCCESS:
		break;
	}

	if (state != WFT_STATE_SUCCESS)
	{
		spdlog::error("Failed. Press Ctrl-C to exit.");
		return;
	}

	std::string name;
	std::string value;

	/* Print request. */
	spdlog::info("request method : {}, version : {}, uri : {}",
				 req->get_method(),
				 req->get_http_version(),
				 req->get_request_uri());
	// 需要说明cursor 中 find()接口会修改cursor内部的指针，即使用过find()过后如果仍然想对header进行遍历，需要通过rewind()接口回到cursor头部。
	protocol::HttpHeaderCursor req_cursor(req);

	while (req_cursor.next(name, value))
		spdlog::info("{} : {}", name, value);

	/* Print response header. */
	spdlog::info("resp version : {}, status code : {}, reqson phrase : {}",
				 resp->get_http_version(),
				 resp->get_status_code(),
				 resp->get_reason_phrase());

	protocol::HttpHeaderCursor resp_cursor(resp);

	while (resp_cursor.next(name, value))
		spdlog::info("{} : {}", name, value);

	/* Print response body. */
	const void *body;
	size_t body_len;

	resp->get_parsed_body(&body, &body_len);

	// write body to file
	FILE *fp = fopen("res.txt", "w");
	fwrite(body, 1, body_len, fp);
	fclose(fp);

	spdlog::info("Success. Press Ctrl-C to exit.");
}

static WFFacilities::WaitGroup wait_group(1);

void sig_handler(int signo)
{
	wait_group.done();
}

int main()
{
	signal(SIGINT, sig_handler);

	std::string url = "www.baidu.com";
	if (strncasecmp(url.c_str(), "http://", 7) != 0 &&
		strncasecmp(url.c_str(), "https://", 8) != 0)
	{
		url = "http://" + url;
	}

	// 创建并启动http任务
	// 所有工厂函数不会返回失败，所以不用担心task为空指针，哪怕是url不合法。一切错误都在callback再处理
	auto task = WFTaskFactory::create_http_task(url,
												REDIRECT_MAX,
												RETRY_MAX,
												wget_callback);
	// req默认是GET方法，HTTP/1.1，长连接。框架会自动加上request_uri(/)，Host(www.baidu.com)等。
	protocol::HttpRequest *req = task->get_req();
	req->add_header_pair("Accept", "*/*");
	req->add_header_pair("User-Agent", "Wget/1.14 (linux-gnu)");
	req->add_header_pair("Connection", "close");
	task->start(); // 非阻塞，并且不会失败
	// 因为异步的原因，start以后显然不能再用task指针了。
	wait_group.wait();
	return 0;
}
