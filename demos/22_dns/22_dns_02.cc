#include <stdio.h>
#include <workflow/WFDnsClient.h>
#include <workflow/WFFacilities.h>

static WFFacilities::WaitGroup wait_group(1);

void sig_handler(int signo)
{
    wait_group.done();
}

int main()
{
    signal(SIGINT, sig_handler);
    std::string url = "http://www.baidu.com";

    auto *task = WFTaskFactory::create_dns_task(url, 0,
    [](WFDnsTask *task)
    {
        auto dns_resp = task->get_resp();
        DnsResultCursor cursor(dns_resp);
		dns_record *record = nullptr;
		while (cursor.next(&record))
		{
			if (record->type == DNS_TYPE_A)
			{
                fprintf(stderr, "ip : %s\n", Util::ip_bin_to_str(record->rdata).c_str());
			}
		}
        fprintf(stderr, "dns done");
    });
    task->start();
    wait_group.wait();
    return 0;
}
