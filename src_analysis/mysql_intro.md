## mysql 

最近发现了一个简单易用，性能非常强大的纯异步mysql客户端，无需依赖任何库

## 简单例子

先看看简单例子如何使用

```cpp
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
```

创建MySQL任务非常简单，有如下几个核心元素

1. url参数，mysql://uname:pwd@host:port/dbname，还支持以SSL连接访问MySQL，mysqls://

2. set_query，执行的SQL语句；

3. callback，异步返回结果的回调

4. 结果集

一个MySQLResponse里包含若干ResultSet，一个ResultSet包含若干Row, 一个Row包含若干Field，很轻松的进行结果集处理，规避掉各种细节。

除了常见的sql的增删改查、建库删库、建表删表、预处理等等，还支持其他的功能特性。

## 多语句多结果集

所有命令都可以拼接到一起通过 set_query() 传给WFMySQLTask（包括INSERT/UPDATE/SELECT/DELETE/PREPARE/CALL）。

举个例子

```cpp
req->set_query("SELECT * FROM table1; CALL procedure1(); INSERT INTO table3 (id) VALUES (1);");
```

在返回的MySQL Response中，有多个ResultSet，遍历即可

## 支持事务

因为MySQL的事务和预处理都是带状态的，一次事务或预处理独占一个连接, 而workflow是高并发异步客户端, 对一个MySQL server的连接可能会不止一个, 我们利用WFMySQLConnection保证独占一个连接。 

见个简单例子 

```cpp
WFMySQLConnection conn(1);
conn.init("mysql://root@127.0.0.1/test");
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
```

我们也可以通过WFMySQLConnection来做预处理PREPARE，因此用户可以很方便地用作防SQL注入。

## 其他功能

作为workflow原生自有协议的一部分，它与任务流，服务治理能特性完美融合，与http，redis，计算任务统一使用，并能够通过upstream轻松实现读写分离。通过workflow灵活的任务流机制，支持MySQL存储过程。

## 高性能

除了易用性外，最为重要的就是workflow的高性能。在合理配置下，每秒能处理几万次MySQL请求。

在传统的MySQL客户端，往往是同步阻塞式的，在向MySQL发送请求到MySQL回复响应的过程中，整个线程处于阻塞等待状态。如果希望提高并发处理能力，往往要设置大量的工作线程，而工作线程的切换，以及临界资源的锁争夺，性能会有较大的影响。

而作为异步MySQL客户端，向MySQL发送请求之后，线程就可以执行本地其他异步调用，或者发送下一个MySQL请求，无任何阻塞，用很少的线程，就能拥有很高的并发处理能力。

![Image](https://pic4.zhimg.com/80/v2-2e5dc1dee6bfb7fe317a033931d277e7.png)