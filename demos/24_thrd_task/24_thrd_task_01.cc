#include <iostream>
#include <workflow/WFTaskFactory.h>
#include <workflow/WFFacilities.h>
// #include <spdlog/spdlog.h>
#include <signal.h>
#include <errno.h>

#include <workflow/logger.h>  // 本版本为了观察内部log而增加，非原生wf组件，请将相关log更换为fprintf

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
	LOG_TRACE("add_routine");
    output->res = input->x + input->y;
}

using AddTask = WFThreadTask<AddInput, AddOutput>;

void callback(AddTask *task)
{
	auto *input = task->get_input();
	auto *output = task->get_output();

	assert(task->get_state() == WFT_STATE_SUCCESS);
	LOG_INFO("%d + %d = %d", input->x, input->y, output->res);
    // spdlog::info("{} + {} = {}", input->x, input->y, output->res);
}

int main()
{
	logger_initConsoleLogger(stdout);
	logger_setLevel(LogLevel_TRACE);

    using AddFactory = WFThreadTaskFactory<AddInput, AddOutput>;
	AddTask *task = AddFactory::create_thread_task("add_task",
												add_routine,
												callback);
	auto *input = task->get_input();

	input->x = 1;
	input->y = 2;

	WFFacilities::WaitGroup wait_group(1);

	Workflow::start_series_work(task, [&wait_group](const SeriesWork *) {
		wait_group.done();
	});

	wait_group.wait();
	return 0;
}

