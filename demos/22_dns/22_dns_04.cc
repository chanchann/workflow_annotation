#include <stdio.h>
#include <workflow/WFDnsClient.h>
#include <workflow/WFFacilities.h>
#include <signal.h>
#include <arpa/inet.h>

static WFFacilities::WaitGroup wait_group(1);
static const int INET_ADDR_LEN = 128;  

void sig_handler(int signo)
{
    wait_group.done();
}

std::string ip_bin_to_str(void* sa_sin_addr)
{
   struct sockaddr_in sa;
   char str[INET_ADDR_LEN];
   inet_ntop(AF_INET, sa_sin_addr, str, INET_ADDR_LEN);
   return str;
}

void dns_callback(WFDnsTask *dns_task) 
{
    auto dns_resp = dns_task->get_resp();
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
            fprintf(stderr, "ipv6 : %s\n", ip_bin_to_str(record->rdata).c_str());
        } 
    }
    fprintf(stderr, "%s request dns done\n", dns_task->get_req()->get_question_name().c_str());
}

int main()
{
    signal(SIGINT, sig_handler);
	WFDnsClient client;
	client.init("dns://119.29.29.29/");
    std::vector<std::string> url_list = {
        "www.baidu.com",
        "www.bing.com",
        "www.tencent.com",
        "www.sogou.com"
    };
    auto first_dns_task = client.create_dns_task("www.zhihu.com", dns_callback);
    SeriesWork *series = Workflow::create_series_work(first_dns_task,
    [](const SeriesWork *series)
    {
        fprintf(stderr, "All tasks have done");
    });
    for(int i = 0; i < 5; i++)
    {
        for(auto url : url_list)
    	{
            series->push_back(client.create_dns_task(url, dns_callback));
    	}
    }
    series->start();
    wait_group.wait();
    return 0;
}
