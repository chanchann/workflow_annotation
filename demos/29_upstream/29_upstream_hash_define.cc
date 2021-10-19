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

框架会使用用户自定义的一致性哈希函数作为data值
*/

/*
实验说明 : 在多个目标中按照框架默认的一致性哈希访问

实验步骤:

1. 先启动29_upstream_server

2. 然后运行此代码，可以看到统计出现的次数

实验结果 :

root@48b37db21b2f:/home/pro/workflow_annotation/demos/build# ./29_upstream_hash_define 
0 : 1000 times  1 : 0 times     2 : 0 times
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
    return WFTaskFactory::create_http_task("http://hash_proxy.name/", 0, 0,
                        std::bind(http_callback, std::placeholders::_1));
}

int main()
{
    std::string proxy_name = "hash_proxy.name";
    // static int upstream_create_consistent_hash(const std::string& name,
    //                                            upstream_route_t consitent_hash);

    UpstreamManager::upstream_create_consistent_hash(proxy_name, 
    [](const char *path, const char *query, const char *fragment) -> unsigned int 
    {
        unsigned int hash = 0;

        while (*path)
            hash = (hash * 131) + (*path++);

        while (*query)
            hash = (hash * 131) + (*query++);

        while (*fragment)
            hash = (hash * 131) + (*fragment++);

        return hash;
    });

    UpstreamManager::upstream_add_server(proxy_name, "127.0.0.1:8000");
    UpstreamManager::upstream_add_server(proxy_name, "127.0.0.1:8001");
    UpstreamManager::upstream_add_server(proxy_name, "127.0.0.1:8002");  
    
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