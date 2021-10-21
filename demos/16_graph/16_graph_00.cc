#include <workflow/WFTaskFactory.h>
#include <workflow/WFGraphTask.h>
#include <workflow/HttpMessage.h>
#include <workflow/WFFacilities.h>

using namespace protocol;

static WFFacilities::WaitGroup wait_group(1);

void go_func(const size_t *size1, const size_t *size2)
{
	fprintf(stderr, "page1 size = %zu, page2 size = %zu\n", *size1, *size2);
}

void http_callback(WFHttpTask *task)
{
	size_t *size = static_cast<size_t *>(task->user_data);
	const void *body;

	if (task->get_state() == WFT_STATE_SUCCESS)
		task->get_resp()->get_parsed_body(&body, size);
	else
		*size = (size_t)-1;
}

#define REDIRECT_MAX	3
#define RETRY_MAX		1

int main()
{
	size_t size1;
	size_t size2;

	WFTimerTask *timer = WFTaskFactory::create_timer_task(1000 * 1000, [](WFTimerTask *) {
		fprintf(stderr, "timer task complete(1s).");
	});

	/* Http task1 */
	WFHttpTask *http_task1 = WFTaskFactory::create_http_task("https://www.sogou.com/",
												 REDIRECT_MAX, RETRY_MAX,
												 http_callback);
	http_task1->user_data = &size1;

	/* Http task2 */
	WFHttpTask *http_task2 = WFTaskFactory::create_http_task("https://www.baidu.com/",
												 REDIRECT_MAX, RETRY_MAX,
												 http_callback);
	http_task2->user_data = &size2;

	/* go task will print the http pages size */
	WFGoTask *go_task = WFTaskFactory::create_go_task("go", go_func, &size1, &size2);

	/* Create a graph. Graph is also a kind of task */
	WFGraphTask *graph = WFTaskFactory::create_graph_task([](WFGraphTask *) {
		fprintf(stderr, "Graph task complete. Wakeup main process");
		wait_group.done();
	});

	/* Create graph nodes */
	WFGraphNode& a = graph->create_graph_node(timer);
	WFGraphNode& b = graph->create_graph_node(http_task1);
	WFGraphNode& c = graph->create_graph_node(http_task2);
	WFGraphNode& d = graph->create_graph_node(go_task);

	/* Build the graph */
	a-->b;
	a-->c;
	b-->d;
	c-->d;

	graph->start();
	wait_group.wait();
	return 0;
}