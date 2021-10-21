#include <workflow/WFTaskFactory.h>
#include <workflow/WFGraphTask.h>
#include <workflow/HttpMessage.h>
#include <workflow/WFFacilities.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <stdlib.h>

using namespace protocol;

static WFFacilities::WaitGroup wait_group(1);

void http_callback(WFHttpTask *task)
{
    HttpResponse *resp = task->get_resp();
    const void *body;
    size_t body_len;
    resp->get_parsed_body(&body, &body_len);
	fprintf(stderr, "[tid : %ld] body len : %zu\n", ::syscall(SYS_gettid), body_len);
}

int main()
{
	
	WFHttpTask *task_01 = WFTaskFactory::create_http_task("http://www.baidu.com", 4, 2,
																		http_callback);
	WFHttpTask *task_02 = WFTaskFactory::create_http_task("http://www.bing.com", 4, 2,
																		http_callback);
    auto pwork1 = Workflow::create_parallel_work(nullptr);
    pwork1->add_series(Workflow::create_series_work(task_01, nullptr));
    pwork1->add_series(Workflow::create_series_work(task_02, nullptr));

	WFHttpTask *task_03 = WFTaskFactory::create_http_task("http://www.zhihu.com", 4, 2,
																		http_callback);
	WFHttpTask *task_04 = WFTaskFactory::create_http_task("http://www.tencent.com", 4, 2,
																		http_callback);
    auto pwork2 = Workflow::create_parallel_work(nullptr);
    pwork2->add_series(Workflow::create_series_work(task_03, nullptr));
    pwork2->add_series(Workflow::create_series_work(task_04, nullptr));

	WFGraphTask *graph = WFTaskFactory::create_graph_task([](WFGraphTask *) {
		fprintf(stderr, "Graph task complete. Wakeup main process");
		wait_group.done();
	});

	WFHttpTask *start_task = WFTaskFactory::create_http_task("http://www.sogou.com", 4, 2,
																		http_callback);
	WFGraphNode& a = graph->create_graph_node(start_task);
	WFGraphNode& b = graph->create_graph_node(pwork1);
	WFGraphNode& c = graph->create_graph_node(pwork2);

	/* Build the graph */
	a-->b;
	a-->c;

	graph->start();
	wait_group.wait();
	return 0;
}