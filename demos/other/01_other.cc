#include <signal.h>

#include <string>
#include <workflow/HttpMessage.h>
#include <workflow/HttpUtil.h>
#include <workflow/WFServer.h>
#include <workflow/WFHttpServer.h>
#include <workflow/WFFacilities.h>
#include <workflow/WFDnsClient.h>
#include <workflow/WFTaskFactory.h>
#include <arpa/inet.h>
#include "util.h"

// 因为返回的页面很小，我们没有关注回复成功与否。http_proxy demo中我们将看到如果获得回复的状态。
void process(WFHttpTask *server_task)
{
    auto http_req = server_task->get_req();
    const char *request_uri = server_task->get_req()->get_request_uri();
    const char *cur = request_uri;
    while (*cur && *cur != '?')
        cur++;

    std::string url(cur + 1);
    fprintf(stderr, "url : %s\n", url.c_str());

    auto dns_task = WFTaskFactory::create_dns_task(url, 4, [](WFDnsTask* dns_task){
		auto dns_resp = dns_task->get_resp();
		DnsResultCursor cursor(dns_resp);
		dns_record *record = nullptr;
		while (cursor.next(&record))
		{
			if (record->type == DNS_TYPE_A)
			{
                fprintf(stderr, "ip : %s\n", util::ip_bin_to_str(record->rdata).c_str());
			}
		}
	});

    auto pwork = Workflow::create_parallel_work([](const ParallelWork *pwork)
    {
        fprintf(stderr, "All series in this parallel have done");
    });
	pwork->add_series(Workflow::create_series_work(dns_task, nullptr));
    
    **server_task << pwork;
}

static WFFacilities::WaitGroup wait_group(1);

void sig_handler(int signo)
{
	wait_group.done();
}

int main()
{
	signal(SIGINT, sig_handler);

	WFHttpServer server(process);
	uint16_t port = 9001;

	if (server.start(port) == 0)
	{
		wait_group.wait();
		server.stop(); 
	}
	else
	{
        fprintf(stderr, "Cannot start server");
		exit(1);
	}

	return 0;
}