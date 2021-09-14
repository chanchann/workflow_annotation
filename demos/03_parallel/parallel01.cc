#include <string.h>
#include <iostream>
#include <utility>
#include <string>
#include <workflow/Workflow.h>
#include <workflow/WFTaskFactory.h>
#include <workflow/HttpMessage.h>
#include <workflow/HttpUtil.h>
#include <workflow/WFFacilities.h>
#include <stdio.h>
using namespace protocol;

#define REDIRECT_MAX 5
#define RETRY_MAX 2

struct tutorial_series_context
{
	std::string url;
	int state;
	int error;
	HttpResponse resp;
};

void callback(const ParallelWork *pwork)
{
	for (size_t i = 0; i < pwork->size(); i++)
	{
		auto ctx = static_cast<tutorial_series_context *>(pwork->series_at(i)->get_context());
		printf("url : %s \t", ctx->url.c_str());
		if (ctx->state == WFT_STATE_SUCCESS)
		{
			const void *body;
			size_t size;
			ctx->resp.get_parsed_body(&body, &size);

			printf("%zu%s\n", size, ctx->resp.is_chunked() ? " chunked" : "");
			// fwrite(body, 1, size, stdout);
			printf("\n");
		}
		else
		{
			printf("ERROR! state = %d, error = %d\n", ctx->state, ctx->error);
		}
		delete ctx; // cb 结束delete
	}
}

int main()
{
	// 创建一个空的并行任务，后续逐个series添加
	ParallelWork *pwork = Workflow::create_parallel_work(callback);
	std::vector<std::string> url_list = {
		"www.baidu.com",
		"www.bing.com",
		"www.github.com"};
	for (auto &url : url_list)
	{
		if (strncasecmp(url.c_str(), "http://", 7) != 0 &&
			strncasecmp(url.c_str(), "https://", 8) != 0)
		{
			url = "http://" + url;
		}

		// 我们先创建http任务，但http任务并不能直接加入到并行任务里，需要先用它创建一个series
		auto task = WFTaskFactory::create_http_task(url, REDIRECT_MAX, RETRY_MAX,
													[](WFHttpTask *http_task)
													{
														// 保存和使用抓取结果
														// 把抓取结果保存在自己的series context里，以便并行任务获取。
														// 这个做法是必须的，因为http任务在callback之后就会被回收，我们只能把resp通过std::move()操作移走。
														// 在上面callback中能很容易的获取
														auto ctx = static_cast<tutorial_series_context *>(series_of(http_task)->get_context());
														ctx->state = http_task->get_state();
														ctx->error = http_task->get_error();
														ctx->resp = std::move(*http_task->get_resp());
													});

		auto req = task->get_req();
		req->add_header_pair("Accept", "*/*");
		req->add_header_pair("User-Agent", "Wget/1.14 (linux-gnu)");
		req->add_header_pair("Connection", "close");

		tutorial_series_context *ctx = new tutorial_series_context;
		ctx->url = std::move(url);
		auto series = Workflow::create_series_work(task, nullptr);
		series->set_context(ctx); // 每个series都带有context，用于保存url和抓取结果

		pwork->add_series(series); // 添加series
	}

	WFFacilities::WaitGroup wait_group(1);

	Workflow::start_series_work(pwork, [&wait_group](const SeriesWork *)
								{ wait_group.done(); });

	wait_group.wait();
	return 0;
}
