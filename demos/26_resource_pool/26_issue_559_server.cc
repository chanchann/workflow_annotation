#include <workflow/WFTask.h>
#include <workflow/WFTaskFactory.h>
#include <workflow/WFResourcePool.h>
#include <workflow/WFFacilities.h>
#include <workflow/WFHttpServer.h>
#include <signal.h>
#include <unordered_map>
#include <memory>
#include "util.h"
#include "json.hpp"

using json = nlohmann::json;

static WFFacilities::WaitGroup wait_group(1);
std::unordered_map<int, std::unique_ptr<WFResourcePool> > uid_pool_map;
std::mutex mutex;

void sig_handler(int signo)
{
    wait_group.done();
}

void go_func(const int uid, const int cnt, protocol::HttpResponse *resp)
{
    char buf[256];
    sprintf(buf, "uid is : %d, cnt is %d\n", uid, cnt);
    resp->append_output_body(buf);
}

int main()
{
    signal(SIGINT, sig_handler);

    WFHttpServer server([](WFHttpTask *server_task)
    {
        auto req = server_task->get_req();
        auto resp = server_task->get_resp();
        const void *body;
        size_t len;
        req->get_parsed_body(&body, &len);

        json js = json::parse(static_cast<const char *>(body));
        
        const int uid = js["uid"];
        const int cnt = js["cnt"];
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (!uid_pool_map.count(uid))
            {
                uid_pool_map[uid] = util::make_unique<WFResourcePool>(1);
            }
        }
     
        WFGoTask *go_task = WFTaskFactory::create_go_task("GO", go_func, uid, cnt,resp);

        WFConditional *cond = uid_pool_map[uid]->get(go_task, &go_task->user_data);
        series_of(server_task)->push_back(cond);

        // 此处有个坑，如果是&uid, 那么此处捕获的uid已经变了
        // 因为这个是在process函数完了再进行, 所以uid已经销毁了，造成segment fault
        // 所以这里先复制一下
        server_task->set_callback([=](WFHttpTask *)
        { 
            uid_pool_map[uid]->post(nullptr); 
            fprintf(stderr, "post resource back\n");
        });
    });

    if (server.start(8888) == 0)
    {
        wait_group.wait();
        server.stop();
    }

    return 0;
}
