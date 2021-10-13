#include <workflow/Workflow.h>
#include <workflow/WFTaskFactory.h>
#include <workflow/WFFacilities.h>
#include <vector>
#include <atomic>
#include <signal.h>

using namespace protocol;

static WFFacilities::WaitGroup wait_group(1);

void sig_handler(int signo)
{
    wait_group.done();
}

std::atomic<int> g_cnt;

int main(int argc, char *argv[])
{
    signal(SIGINT, sig_handler);
	if (argc != 2)
	{
		fprintf(stderr, "USAGE: %s <max parallel number>\n", argv[0]);
		exit(1);
	}
    int max_para = atoi(argv[1]); // 设置为1，那么则就变成了串行了

    WFResourcePool pool(max_para);
    std::vector<std::string> urls = {
        "http://www.baidu.com",
        "http://www.bing.com",
        "http://www.sogou.com"
    };

    WFFacilities::WaitGroup wait_group(3);

    for (auto &url : urls)
    {
        WFHttpTask *task = WFTaskFactory::create_http_task(url, 4, 2,
        [&pool, &wait_group, &url](WFHttpTask *task)
        {
            fprintf(stderr, "%d - url[%s]\n", g_cnt++, url.c_str());
            pool.post(nullptr); 
            wait_group.done();
        });
        // 新版代码中无需保存res，可以不传resbuf参数。
        // 多了一个这个接口 : WFConditional *get(SubTask *task);
        // WFConditional *cond = pool.get(task); 
        // 我们旧版这个代码没有这个，所以随便拿个来装一下
        WFConditional *cond = pool.get(task, &task->user_data); 
        cond->start();
    }
    wait_group.wait();

    return 0;
}
