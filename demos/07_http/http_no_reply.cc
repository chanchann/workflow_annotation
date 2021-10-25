
#include <workflow/WFHttpServer.h>
#include <workflow/WFFacilities.h>
#include <signal.h>

/*
curl -i "http://127.0.0.1:8888/noreply"
curl: (52) Empty reply from server
*/
static WFFacilities::WaitGroup wait_group(1);

void sig_handler(int signo)
{
    wait_group.done();
}

int main()
{
    signal(SIGINT, sig_handler);
    WFHttpServer server([](WFHttpTask *task) { 
        task->set_callback([](WFHttpTask *task) {
            // todo : 如何查看连接已经断开
            // state : WFT_STATE_NOREPLY
            fprintf(stderr, "task finished, state = %d\n", task->get_state());
        });
        if (strcmp(task->get_req()->get_request_uri(), "/noreply") == 0)
        {
            task->noreply();
            fprintf(stderr, "task set to no reply");
            // 查看下面的resp有用吗
            task->get_resp()->append_output_body("<html>server reply</html>");
            return;
        }
        else
        {
            task->get_resp()->append_output_body("<html>server test</html>");
            return;
        }
    });
    
    if (server.start(8888) == 0) {
        wait_group.wait();
        server.stop();
    }

    return 0;
}
