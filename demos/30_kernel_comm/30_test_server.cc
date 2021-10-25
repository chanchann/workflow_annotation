
#include <workflow/WFHttpServer.h>
#include <workflow/WFFacilities.h>
#include <signal.h>

static WFFacilities::WaitGroup wait_group(1);

void sig_handler(int signo)
{
    wait_group.done();
}

int main()
{
    signal(SIGINT, sig_handler);
    WFHttpServer server([](WFHttpTask *task) { 
        task->get_resp()->append_output_body("<html>server test</html>");
        return;
    });
    
    if (server.start(8888) == 0) {
        wait_group.wait();
        server.stop();
    }

    return 0;
}
