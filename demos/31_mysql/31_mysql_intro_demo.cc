#include "workflow/Workflow.h"
#include "workflow/WFTaskFactory.h"
#include "workflow/MySQLResult.h"

using namespace protocol;

int main()
{
    std::string url = "mysql://root:123@localhost";
    WFMySQLTask *task = WFTaskFactory::create_mysql_task(url, 0, [](WFMySQLTask *task)
    {
        MySQLResponse *resp = task->get_resp();
        MySQLResultCursor cursor(resp);
        std::vector<MySQLCell> arr;
        while (cursor.fetch_row(arr))
        {
            fprintf(stderr, "[%s]\n", arr[0].as_string().c_str());
        }
    });
    task->get_req()->set_query("SHOW DATABASES");
    task->start();
    getchar();
}