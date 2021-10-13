#include <workflow/WFTask.h>
#include <workflow/WFTaskFactory.h>
#include <workflow/WFResourcePool.h>
#include <workflow/WFFacilities.h>

int main() {
	int res_concurrency = 3;
	int task_concurrency = 10;
	const char *words[3] = {"http", "mysql", "redis"};
	WFResourcePool res_pool((void * const*)words, res_concurrency);

	WFFacilities::WaitGroup wg(task_concurrency);

	for (int i = 0; i < task_concurrency; i++)
	{
		auto *user_task = WFTaskFactory::create_timer_task(0,
		[&wg, &res_pool](WFTimerTask *task) {
			uint64_t id = (uint64_t)series_of(task)->get_context();
			printf("task-%lu get [%s]\n", id, (char *)task->user_data);
			res_pool.post(task->user_data);
			wg.done();
		});

		auto *cond = res_pool.get(user_task, &user_task->user_data);   // 用user_data来保存res是一种实用方法。

		SeriesWork *series = Workflow::create_series_work(cond, nullptr);
		series->set_context(reinterpret_cast<uint64_t *>(i));
		series->start();
	}

	wg.wait();
    return 0;
}