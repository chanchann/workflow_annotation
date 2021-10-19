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

随机选择一个目标
如果try_another配置为true，那么将在所有存活的目标中随机选择一个
仅在main中选择，选中目标所在group的主备和无group的备都视为有效的可选对象
*/

/*
实验1说明 : 在多个目标中随机访问

配置一个本地反向代理，将本地发出的random_proxy.name所有请求均匀的打到3个目标server上

实验步骤:

1. 先启动29_upstream_server

2. 然后运行此代码，可以看到统计出现的次数

todo: 这里有个问题

这个貌似不是真正的概率上random了，

root@48b37db21b2f:/home/pro/workflow_annotation/demos/build# ./29_upstream_random 
0 : 354 times   1 : 305 times   2 : 341 times
root@48b37db21b2f:/home/pro/workflow_annotation/demos/build# ./29_upstream_random 
0 : 354 times   1 : 305 times   2 : 341 times
root@48b37db21b2f:/home/pro/workflow_annotation/demos/build# ./29_upstream_random 
0 : 354 times   1 : 305 times   2 : 341 times

每次跑出的统计结构都一样，看下源码分析下
*/

// global for convinience
const int server_num = 3;   // 和server那边匹配上
const int times = 1000;
WFFacilities::WaitGroup wait_group(times);
std::vector<int> cnt_list(server_num, 0);   // 统计一下

void http_callback(WFHttpTask* task)
{
    // fprintf(stderr, "req : %s\n", task->get_req()->get_request_uri());
    const void *body;  
    size_t body_len;
    task->get_resp()->get_parsed_body(&body, &body_len);
    std::string body_str(static_cast<const char *>(body));
    // fprintf(stderr, "%s\n", body_str.c_str());
    auto body_split = StringUtil::split(body_str, '-');
    int num = atoi(body_split.back().c_str());
    // fprintf(stderr, "num : %d\n", num);
    cnt_list[num]++;
    // fprintf(stderr, "task done\n");
    wait_group.done();
}

WFHttpTask* create_http_task()
{
    return WFTaskFactory::create_http_task("http://random_proxy.name/", 0, 0,
                        std::bind(http_callback, std::placeholders::_1));
}

int main()
{
    // static int upstream_create_weighted_random(const std::string& name,
                                            //    bool try_another);
    std::string proxy_name = "random_proxy.name";
    // try_another = true : 如果遇到熔断机器，再次尝试直至找到可用或全部熔断
    UpstreamManager::upstream_create_weighted_random(proxy_name, true);  

    UpstreamManager::upstream_add_server(proxy_name, "127.0.0.1:8000");
    UpstreamManager::upstream_add_server(proxy_name, "127.0.0.1:8001");
    UpstreamManager::upstream_add_server(proxy_name, "127.0.0.1:8002");

    // todo : 域名另做实验，此处只是统计
    // UpstreamManager::upstream_add_server(proxy_name, "www.baidu.com");
    // todo : in what condition use this?
    // UpstreamManager::upstream_add_server(proxy_name, "/dev/unix_domain_scoket_sample");
    
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