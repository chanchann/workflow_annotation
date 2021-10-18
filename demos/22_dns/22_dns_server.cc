#include <workflow/WFDnsServer.h>
#include <signal.h>
#include <string>
#include <workflow/WFFacilities.h>
#include <arpa/inet.h>

using namespace protocol;

void process(WFDnsTask *dns_task)
{
    DnsRequest *req = dns_task->get_req();
    DnsResponse *resp = dns_task->get_resp();

}

static WFFacilities::WaitGroup wait_group(1);

void sig_handler(int signo)
{
	wait_group.done();
}

int main()
{
	signal(SIGINT, sig_handler);

	WFDnsServer server(process);
	uint16_t port = 8888;

	if (server.start(port) == 0)
	{
		wait_group.wait();
		server.stop(); // 关停是非暴力式的，会等待正在服务的请求执行完。
	}
	else
	{
        fprintf(stderr, "Cannot start server");
		exit(1);
	}

	return 0;
}
