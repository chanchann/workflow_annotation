// WFDynamicTask，通过一个函数在它实际启动的时候再生成具体的task。
// 可以把一切任务变成动态创建，避免callback

#include <iostream>
#include <workflow/WFFacilities.h>
#include <workflow/WFTaskFactory.h>

WFFacilities::WaitGroup wait_group(1);

SubTask *func(WFDynamicTask *)
{
    std::string url;
    std::cin >> url;
    return WFTaskFactory::create_http_task(url, 3, 0, [](WFHttpTask *task) {
        const void *body;
        size_t size;

        if (task->get_resp()->get_parsed_body(&body, &size))
            std::cout << static_cast<const char *>(body);

        wait_group.done();
    });
}

int main()
{
    WFDynamicTask *t = WFTaskFactory::create_dynamic_task(func);
    t->start();
    wait_group.wait();
    return 0;
}