#include <workflow/Workflow.h>
#include <workflow/WFTaskFactory.h>
#include <workflow/WFFacilities.h>

#include <functional>
#include <iostream>
#include "message.h"

using WFEchoTask = WFNetworkTask<protocol::EchoRequest,
								protocol::EchoResponse>;
using echo_callback_t = std::function<void (WFEchoTask *)>;

using namespace protocol;

class EchoFactory : public WFTaskFactory {
public:
	static WFEchoTask *create_echo_task(const std::string& host,
                                        unsigned short port,
                                        int retry_max,
                                        echo_callback_t callback) {
		using NTF = WFNetworkTaskFactory<EchoRequest, EchoResponse>;
		WFEchoTask *task = NTF::create_client_task(TT_TCP, host, port,
                                                    retry_max,
                                                    std::move(callback));
		task->set_keep_alive(30 * 1000);
		return task;
	}
};

int main() {
    uint16_t port = 9999;
	std::string host = "127.0.0.1";

	std::function<void (WFEchoTask *task)> callback =
		[&host, port, &callback](WFEchoTask *task) {
		int state = task->get_state();
		int error = task->get_error();
		EchoResponse *resp = task->get_resp();
		std::string body;
		size_t bodySize;

		if (state != WFT_STATE_SUCCESS) {
			if (state == WFT_STATE_SYS_ERROR)
				std::cerr << "SYS error : " << strerror(error) << std::endl;
			else if (state == WFT_STATE_DNS_ERROR)
				std::cerr << "DNS error : " << gai_strerror(error) << std::endl;
			else
				std::cerr << "other error" << std::endl;
			return;
		}

		resp->get_message_body_nocopy(&body, &bodySize);
		if (bodySize != 0)
			std::cout << "Sercer Resp : " << bodySize << " : " << body << std::endl;

		std::cout << "Input next request string (Ctrl-D to exit): " << std::endl;
		std::string buf;

		std::cin >> buf;
		if (!buf.empty()) {
			WFEchoTask *next;
			next = EchoFactory::create_echo_task(host, port, 0, callback);
			next->get_req()->set_message_body(buf);
			next->get_resp()->set_size_limit(4 * 1024);
			**task << next; /* equal to: series_of(task)->push_back(next) */
		}
		else {
			std::cout << std::endl;
		}
			
	};

	/* First request is emtpy. We will ignore the server response. */
	WFFacilities::WaitGroup wait_group(1);
	WFEchoTask *task = EchoFactory::create_echo_task(host, port, 0, callback);
	task->get_resp()->set_size_limit(4 * 1024);
	Workflow::start_series_work(task, [&wait_group](const SeriesWork *) {
		wait_group.done();
	});

	wait_group.wait();
	return 0;
}
