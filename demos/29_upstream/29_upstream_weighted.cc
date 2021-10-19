#include <workflow/UpstreamManager.h>
#include <workflow/WFHttpServer.h>
#include <workflow/WFTaskFactory.h>
#include <workflow/WFFacilities.h>
#include <workflow/UpstreamPolicies.h>
#include <workflow/StringUtil.h>
#include <vector>
#include <string>

/*
基本原理

按照权重分配，随机选择一个目标，权重越大概率越大
如果try_another配置为true，那么将在所有存活的目标中按照权重分配随机选择一个
仅在main中选择，选中目标所在group的主备和无group的备都视为有效的可选对象
*/

/*
实验说明 : 在多个目标中按照权重大小随机访问

实验步骤:

1. 先启动29_upstream_server

2. 然后运行此代码，可以看到统计出现的次数

实验结果 :

root@48b37db21b2f:/home/pro/workflow_annotation/demos/build# ./29_upstream_weighted
0 : 301 times   1 : 593 times   2 : 106 times

大概为 3 : 6 : 1

还是那个问题，每次跑都是固定的数
*/

// global for convinience
const int server_num = 3;   // 和server那边匹配上
const int times = 1000;
WFFacilities::WaitGroup wait_group(times);
std::vector<int> cnt_list(server_num, 0);   // 统计一下

void http_callback(WFHttpTask* task)
{
    const void *body;  
    size_t body_len;
    task->get_resp()->get_parsed_body(&body, &body_len);
    std::string body_str(static_cast<const char *>(body));
    auto body_split = StringUtil::split(body_str, '-');
    int num = atoi(body_split.back().c_str());
    cnt_list[num]++;
    wait_group.done();
}

WFHttpTask* create_http_task()
{
    return WFTaskFactory::create_http_task("http://weighted_proxy.name/", 0, 0,
                        std::bind(http_callback, std::placeholders::_1));
}

int main()
{
    std::string proxy_name = "weighted_proxy.name";
    // try_another = fales : 如果遇到熔断机器，不再尝试，这种情况下此次请求必定失败
    UpstreamManager::upstream_create_weighted_random(proxy_name, false);  
    AddressParams address_params = ADDRESS_PARAMS_DEFAULT;
    address_params.weight = 3;  // 权重为3
    UpstreamManager::upstream_add_server(proxy_name, "127.0.0.1:8000", &address_params);
    address_params.weight = 6;  // 权重为6
    UpstreamManager::upstream_add_server(proxy_name, "127.0.0.1:8001", &address_params);
    UpstreamManager::upstream_add_server(proxy_name, "127.0.0.1:8002");  // 权重1
    
    auto first_task = create_http_task();
    auto series = Workflow::create_series_work(first_task, nullptr);
    for(int i = 0; i < times - 1; i++)
    {
        series->push_back(create_http_task());
    }
    series->start();
    wait_group.wait();
    for(int i = 0; i < server_num; i++)
    {
        fprintf(stderr, "%d : %d times\t", i, cnt_list[i]);
    }
    fprintf(stderr, "\n");
    return 0;
}