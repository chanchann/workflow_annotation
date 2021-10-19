#include <workflow/UpstreamManager.h>
#include <workflow/WFHttpServer.h>
#include <workflow/WFTaskFactory.h>
#include <workflow/WFFacilities.h>
#include <workflow/UpstreamPolicies.h>
#include <vector>
#include <string>

void basic_callback(WFHttpTask *task, std::string message)
{
	if (task->get_state() == WFT_STATE_SUCCESS)
	{
		const void *body;
		size_t body_len;
		task->get_resp()->get_parsed_body(&body, &body_len);
		std::string buffer(static_cast<const char *>(body), body_len);
		fprintf(stderr, "buffer : %s\n", buffer.c_str());
		fprintf(stderr, "message : %s\n", message.c_str());
	}
	WFFacilities::WaitGroup *wait_group = 
		static_cast<WFFacilities::WaitGroup *>(task->user_data);
	wait_group->done();
}

int main()
{
	WFFacilities::WaitGroup wait_group(3);
	std::vector<std::string> url_list = {"http://weighted.random", "http://hash", "http://manual"};

	http_callback_t cb = std::bind(basic_callback, std::placeholders::_1,
								   std::string("server1"));
	for (int i = 0; i < 3; i++)
	{
		WFHttpTask *task = WFTaskFactory::create_http_task(url_list[i],
											  2, 2, cb);
		task->user_data = &wait_group;
		task->start();
	}

	wait_group.wait();
}