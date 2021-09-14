#include <ctype.h>
#include <signal.h>
#include <workflow/Workflow.h>
#include <workflow/WFTaskFactory.h>
#include <workflow/WFServer.h>
#include <workflow/WFFacilities.h>
#include <iostream>
#include "message.h"

using WFEchoTask = WFNetworkTask<protocol::EchoRequest,
								 protocol::EchoResponse>;
using WFEchoServer = WFServer<protocol::EchoRequest,
							  protocol::EchoResponse>;

using namespace protocol;

void process(WFEchoTask *task)
{
	EchoRequest *req = task->get_req();
	EchoResponse *resp = task->get_resp();
	std::string body;
	size_t size = 0;

	req->get_message_body_nocopy(&body, &size);

	for (size_t i = 0; i < size; i++)
	{
		body[i] = toupper(body[i]);
	}

	resp->set_message_body(body);
}

static WFFacilities::WaitGroup wait_group(1);

void sig_handler(int signo)
{
	wait_group.done();
}

int main()
{
	uint16_t port = 9999;

	signal(SIGINT, sig_handler);

	struct WFServerParams params = SERVER_PARAMS_DEFAULT;
	params.request_size_limit = 4 * 1024;

	WFEchoServer server(&params, process);
	if (server.start(AF_INET6, port) == 0 ||
		server.start(AF_INET, port) == 0)
	{
		wait_group.wait();
		server.stop();
	}
	else
	{
		std::cerr << "server.start" << std::endl;
		exit(1);
	}

	return 0;
}