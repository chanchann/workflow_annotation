#include <workflow/Workflow.h>
#include <workflow/WFTaskFactory.h>
#include <workflow/WFFacilities.h>

int main()
{
    ParallelWork *pwork = Workflow::create_parallel_work(nullptr);
    for(int i = 0; i < 3; i++)
    {
        WFTimerTask *task = WFTaskFactory::create_timer_task(3000 * 1000,    
        [](WFTimerTask *timer)
        {
            fprintf(stderr, "timer end\n");
        });
        SeriesWork *series = Workflow::create_series_work(task, nullptr);
        pwork->add_series(series);
    }
    pwork->start();
    getchar();
    return 0;
}