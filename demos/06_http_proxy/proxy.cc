#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <utility>
#include <workflow/Workflow.h>
#include <workflow/HttpMessage.h>
#include <workflow/HttpUtil.h>
#include <workflow/WFHttpServer.h>
#include <workflow/WFFacilities.h>

#include <spdlog/spdlog.h>

// 这个proxy在实现上需要抓取下来完整的http页面再转发，下载/上传大文件会有延迟
// 本质上是将用户请求原封不动转发到对应的web server，再将web server的回复原封不动转发给用户

// 浏览器发给proxy的请求里，request uri包含了scheme和host，port，转发时需要去除。
// 例如，访问http://www.sogou.com/， 浏览器发送给proxy请求首行是：
// GET http://www.sogou.com/ HTTP/1.1 需要改写为：
// GET / HTTP/1.1

// 回复消息的时机是在series里所有其它任务被执行完后，自动回复，所以并没有task->reply()接口。

// usage : curl -x http://localhost:8888/ http://www.baidu.com
struct tutorial_series_context
{
	std::string url; // url主要是后续打日志之用
	WFHttpTask *proxy_task;
	bool is_keep_alive;
};

// 在这里只为了打印一些log
// 在这个callback执行结束后，proxy task会被自动delete。
void reply_callback(WFHttpTask *proxy_task)
{
	SeriesWork *series = series_of(proxy_task);
	tutorial_series_context *context =
		static_cast<tutorial_series_context *>(series->get_context());
	auto *proxy_resp = proxy_task->get_resp();
	size_t size = proxy_resp->get_output_body_size();

	if (proxy_task->get_state() == WFT_STATE_SUCCESS)
		spdlog::info("{}: Success. Http Status: {}, BodyLength: {}",
					 context->url, proxy_resp->get_status_code(), size);
	else /* WFT_STATE_SYS_ERROR*/
		spdlog::info("{}: Reply failed: {}, BodyLength: {}",
					 context->url, strerror(proxy_task->get_error()), size);
}

// web server响应的部分
void http_callback(WFHttpTask *task)
{
	int state = task->get_state();
	int error = task->get_error();
	auto *resp = task->get_resp();
	SeriesWork *series = series_of(task);
	tutorial_series_context *context =
		static_cast<tutorial_series_context *>(series->get_context());
	auto *proxy_resp = context->proxy_task->get_resp();

	/* Some servers may close the socket as the end of http response. */
	if (state == WFT_STATE_SYS_ERROR && error == ECONNRESET)
	{
		state = WFT_STATE_SUCCESS;
	}

	if (state == WFT_STATE_SUCCESS)
	{
		const void *body;
		size_t len;

		/* set a callback for getting reply status. */
		context->proxy_task->set_callback(reply_callback);

		/* Copy the remote webserver's response, to proxy response. */
		if (resp->get_parsed_body(&body, &len))
		{
			resp->append_output_body_nocopy(body, len);
		}
		*proxy_resp = std::move(*resp);
		if (!context->is_keep_alive)
		{
			proxy_resp->set_header_pair("Connection", "close");
		}
	}
	else
	{ // not success
		const char *err_string;
		int error = task->get_error();

		if (state == WFT_STATE_SYS_ERROR)
			err_string = strerror(error);
		else if (state == WFT_STATE_DNS_ERROR)
			err_string = gai_strerror(error);
		else if (state == WFT_STATE_SSL_ERROR)
			err_string = "SSL error";
		else /* if (state == WFT_STATE_TASK_ERROR) */
			err_string = "URL error (Cannot be a HTTPS proxy)";

		spdlog::info("{}: Fetch failed. state = {}, error = {}: {}",
					 context->url, state, task->get_error(), err_string);

		/* As a tutorial, make it simple. And ignore reply status. */
		// 此处所有失败的情况，简单返回一个404页面
		proxy_resp->set_status_code("404");
		proxy_resp->append_output_body_nocopy(
			"<html>404 Not Found.</html>", 27);
	}
}

void process(WFHttpTask *proxy_task)
{
	// 先解析向web server发送的http请求的构造
	auto *req = proxy_task->get_req();
	// 任何task都是SubTask类型的派生。
	// 而任何运行中的task，一定属于某个series。通过series_of调用，得到了任务所在的series。
	SeriesWork *series = series_of(proxy_task);

	tutorial_series_context *context = new tutorial_series_context;
	context->url = req->get_request_uri(); // req->get_request_uri()调用得到浏览器请求的完整URL，通过这个URL构建发往server的http任务。
	context->proxy_task = proxy_task;	   // context里包含了proxy_task本身，以便我们异步填写结果。

	series->set_context(context);
	series->set_callback([](const SeriesWork *series)
						 {
							 // 最后，series的callback里销毁context。
							 delete static_cast<tutorial_series_context *>(series->get_context());
						 });

	context->is_keep_alive = req->is_keep_alive();
	// for requesting remote webserver.
	// 这个http任务重试与重定向次数都是0，因为重定向是由浏览器处理，遇到302等会重新发请求。
	auto http_task = WFTaskFactory::create_http_task(req->get_request_uri(), 0, 0,
													 http_callback);

	const void *body;
	size_t len;

	/* Copy user's request to the new task's reuqest using std::move() */
	// 生成发往web server的http请求
	req->set_request_uri(http_task->get_req()->get_request_uri()); // 会将request_uri里的http://host:port部分去掉，只保留path之后的部分
	req->get_parsed_body(&body, &len);							   // 解析下来的http body指定为向外输出的http body，无需拷贝
	req->append_output_body_nocopy(body, len);
	*http_task->get_req() = std::move(*req); // req是我们收到的http请求，我们最终要通过std::move()把它直接移动到新请求上。

	/* also, limit the remote webserver response size. */
	http_task->get_resp()->set_size_limit(200 * 1024 * 1024);

	*series << http_task; // 构造好http请求后，将这个请求放到当前series末尾，process函数结束。
}

static WFFacilities::WaitGroup wait_group(1);

void sig_handler(int signo)
{
	wait_group.done();
}

int main()
{
	unsigned short port = 8888;
	signal(SIGINT, sig_handler);

	struct WFServerParams params = HTTP_SERVER_PARAMS_DEFAULT;
	/* for safety, limit request size to 8MB. */
	// 防止被恶意攻击
	params.request_size_limit = 8 * 1024 * 1024;

	WFHttpServer server(&params, process);

	if (server.start(port) == 0)
	{
		wait_group.wait();
		server.stop();
	}
	else
	{
		spdlog::critical("Cannot start server");
		exit(1);
	}
	return 0;
}
