#include <workflow/Workflow.h>
#include <workflow/WFTaskFactory.h>
#include <workflow/WFFacilities.h>

int main()
{
    WFTimerTask *task = WFTaskFactory::create_timer_task(3000 * 1000,    
    [](WFTimerTask *timer)
    {
        fprintf(stderr, "timer end\n");
    });

    task->start();
    getchar();
    return 0;
}