#include <stdio.h>
#include <workflow/WFDnsClient.h>
#include <workflow/WFFacilities.h>

static WFFacilities::WaitGroup wait_group(1);

void sig_handler(int signo)
{
    wait_group.done();
}

int main() {
	
	unsigned short req_id = 0x4321;
	WFFacilities::WaitGroup wait_group(1);
	WFDnsClient client;

	client.init("dns://119.29.29.29/");

	auto *task = client.create_dns_task("www.sogou.com",
	[&wait_group, req_id] (WFDnsTask *task)
	{
		int state = task->get_state();

		if (state == WFT_STATE_SUCCESS)
		{
			unsigned short resp_id = task->get_resp()->get_id();
            if(req_id == resp_id) {
                fprintf(stderr, "id equals\n");
            }
		}
		wait_group.done();
	});

	client.deinit();

	auto *req = task->get_req();
	req->set_id(req_id);
	task->start();

    wait_group.wait();
}
