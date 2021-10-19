#include <workflow/UpstreamManager.h>
#include <workflow/WFHttpServer.h>
#include <workflow/WFTaskFactory.h>
#include <workflow/WFFacilities.h>
#include <workflow/UpstreamPolicies.h>
#include <vector>
#include <string>

/*
基本原理

随机选择一个目标
如果try_another配置为true，那么将在所有存活的目标中随机选择一个
仅在main中选择，选中目标所在group的主备和无group的备都视为有效的可选对象
*/

int main()
{
    WFFacilities::WaitGroup wait_group(1);
    // static int upstream_create_weighted_random(const std::string& name,
                                            //    bool try_another);
    UpstreamManager::upstream_create_weighted_random(
        "random_proxy.name",
        true);  //如果遇到熔断机器，再次尝试直至找到可用或全部熔断

    UpstreamManager::upstream_add_server("random_proxy.name", "127.0.0.1:8000");
    UpstreamManager::upstream_add_server("random_proxy.name", "127.0.0.1:8001");
    UpstreamManager::upstream_add_server("random_proxy.name", "127.0.0.1:8002");
    UpstreamManager::upstream_add_server("random_proxy.name", "www.sogou.com");
    // UpstreamManager::upstream_add_server("random_proxy.name", "/dev/unix_domain_scoket_sample");

    auto http_task = WFTaskFactory::create_http_task("http://random_proxy.name/", 0, 0, 
    [&wait_group](WFHttpTask* task){
        fprintf(stderr, "task done");
        wait_group.done();
    });
    http_task->start();
    wait_group.wait();
    return 0;
}