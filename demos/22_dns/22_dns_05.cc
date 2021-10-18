#include <stdio.h>
#include <workflow/WFDnsClient.h>
#include <workflow/WFFacilities.h>
#include <signal.h>
#include <arpa/inet.h>

static WFFacilities::WaitGroup wait_group(1);
static const int INET_ADDR_LEN = 128;  


// 获取ipv6

void sig_handler(int signo)
{
    wait_group.done();
}

std::string ip_bin_to_str(void* sa_sin_addr, bool ipv4 = true)
{
    struct sockaddr_in sa;
    char str[INET_ADDR_LEN];
    if(ipv4)
        inet_ntop(AF_INET, sa_sin_addr, str, INET_ADDR_LEN);
    else 
        inet_ntop(AF_INET6, sa_sin_addr, str, INET_ADDR_LEN);
    return str;
}

int main()
{
    signal(SIGINT, sig_handler);
    std::string url = "dns://8.8.8.8/ipv6.tsinghua.edu.cn";

    auto *task = WFTaskFactory::create_dns_task(url, 4,
    [](WFDnsTask *task)
    {
        auto dns_resp = task->get_resp();
        DnsResultCursor cursor(dns_resp);
		dns_record *record = nullptr;
		while (cursor.next(&record))
		{
			if (record->type == DNS_TYPE_A)
			{
                fprintf(stderr, "ipv4 : %s\n", ip_bin_to_str(record->rdata).c_str());
			} 
            else if(record->type == DNS_TYPE_AAAA) 
            {
                fprintf(stderr, "ipv6 : %s\n", ip_bin_to_str(record->rdata, false).c_str());
            } 
		}
        fprintf(stderr, "dns done\n");
    });

    task->get_req()->set_question_type(DNS_TYPE_AAAA);  // 只需要 set_question_type 即可
    
    task->start();
    wait_group.wait();
    return 0;
}
