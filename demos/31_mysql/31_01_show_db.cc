#include "workflow/Workflow.h"
#include "workflow/WFTaskFactory.h"
#include "workflow/MySQLResult.h"

using namespace protocol;

int main()
{
    // mysql://username:password@host:port/dbname?character_set=charset&character_set_results=charset
    std::string url = "mysql://root:123@localhost";
    WFMySQLTask *task = WFTaskFactory::create_mysql_task(url, 0, [](WFMySQLTask *task)
    {
        // 通过MySQLResultCursor遍历结果集
        MySQLResponse *resp = task->get_resp();
        MySQLResultCursor cursor(resp);
        const MySQLField *const *fields;

        // 判断任务状态 
        if (task->get_state() != WFT_STATE_SUCCESS)
        {
            fprintf(stderr, "error msg: %s\n",
                    WFGlobal::get_error_string(task->get_state(),
                                            task->get_error()));
            return;
        }
        // 一次请求所对应的回复中，其数据是一个三维结构：
        // 一个回复中包含了一个或多个结果集（result set）；
        // 一个结果集的类型可能是MYSQL_STATUS_GET_RESULT或者MYSQL_STATUS_OK；
        // MYSQL_STATUS_GET_RESULT类型的结果集包含了一行或多行（row）；
        // 一行包含了一列或多个列，或者说一到多个阈（Field/Cell），具体数据结构为MySQLField和MySQLCell；
        if (cursor.get_cursor_status() == MYSQL_STATUS_GET_RESULT)
        {
            fprintf(stderr, "cursor_status=%d field_count=%u rows_count=%u\n",
                        cursor.get_cursor_status(), cursor.get_field_count(),
                        cursor.get_rows_count());
        }
        fprintf(stderr, "databases list :\n");
        std::vector<MySQLCell> arr;
        while (cursor.fetch_row(arr))
        {
            fprintf(stderr, "[%s]\n", arr[0].as_string().c_str());
        }
    });

    // 如果没调用过 set_query() ，task就被start起来的话，则用户会在callback里得到WFT_ERR_MYSQL_QUERY_NOT_SET。
    // 交互命令中不支持选库（USE命令)
    task->get_req()->set_query("SHOW DATABASES");
    task->start();
    getchar();
}