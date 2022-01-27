#include "workflow/WFMySQLConnection.h"
#include "workflow/WFOperator.h"

int id = 0;

void task_callback(WFMySQLTask *task)
{
    fprintf(stderr, "t%d\n", ++id);
}

int main()
{
    WFMySQLConnection conn(1);
    conn.init("mysql://root@127.0.0.1/wfrest_test");

    // test transaction
    const char *query = "BEGIN;";
    WFMySQLTask *t1 = conn.create_query_task(query, task_callback);
    query = "SELECT * FROM check_tiny FOR UPDATE;";
    WFMySQLTask *t2 = conn.create_query_task(query, task_callback);
    query = "INSERT INTO check_tiny VALUES (8);";
    WFMySQLTask *t3 = conn.create_query_task(query, task_callback);
    query = "COMMIT;";
    WFMySQLTask *t4 = conn.create_query_task(query, task_callback);
    WFMySQLTask *t5 = conn.create_disconnect_task(task_callback);

    ((*t1) > t2 > t3 > t4 > t5).start();
    getchar();
}

