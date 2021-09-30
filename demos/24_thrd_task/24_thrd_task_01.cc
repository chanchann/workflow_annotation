#include <iostream>
#include <workflow/WFTaskFactory.h>
#include <workflow/WFFacilities.h>
#include <spdlog/spdlog.h>
#include <signal.h>
#include <errno.h>

// 直接定义thread_task三要素

// 定义INPUT
struct AddInput
{
    int x;
    int y;
};

// 定义OUTPUT
struct AddOutput
{
    int res;
};

// 加法流程
void add_routine(const AddInput *input, AddOutput *output)
{
    output->res = input->x + input->y;
}

using AddTask = WFThreadTask<AddInput, AddOutput>;

void callback(AddTask *task)
{
	auto *input = task->get_input();
	auto *output = task->get_output();

	assert(task->get_state() == WFT_STATE_SUCCESS);

	if (output->error)
        spdlog("Error : {} - {}", output->error, strerror(output->error));
	else
	{
        spdlog("{} + {} = {}", input->x, input->y, output->res);
	}
}

int main()
{
    
	AddTask *task = MMFactory::create_thread_task("add_task",
												add_routine,
												callback);
	auto *input = task->get_input();

	input->a = 1;
	input->b = 2;

	WFFacilities::WaitGroup wait_group(1);

	Workflow::start_series_work(task, [&wait_group](const SeriesWork *) {
		wait_group.done();
	});

	wait_group.wait();
	return 0;
}

