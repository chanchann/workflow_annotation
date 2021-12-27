#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <vector>
#include <map>
#include "workflow/Workflow.h"
#include "workflow/WFTaskFactory.h"
#include "workflow/MySQLResult.h"
#include "workflow/WFFacilities.h"

static const int RETRY_MAX = 0;
volatile bool stop_flag;

static void sighandler(int signo)
{
	stop_flag = true;
}

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		fprintf(stderr, "USAGE: %s <url>\n"
				"      url format: mysql://root:password@host:port/dbname?character_set=charset\n"
				"      example: mysql://root@test.mysql.com/test\n",
				argv[0]);
		return 0;
	}

	signal(SIGINT, sighandler);
    // SIGTERM是杀或的killall命令发送到进程默认的信号
	signal(SIGTERM, sighandler);

	std::string url = argv[1];
	if (strncasecmp(argv[1], "mysql://", 8) != 0 &&
		strncasecmp(argv[1], "mysqls://", 9) != 0)
	{
		url = "mysql://" + url;
	}
	std::string a;
	const char *query = "show databases";
	stop_flag = false;

	WFMySQLTask *task = WFTaskFactory::create_mysql_task(url, RETRY_MAX, mysql_callback);
	task->get_req()->set_query(query);

	WFFacilities::WaitGroup wait_group(1);
	SeriesWork *series = Workflow::create_series_work(task,
		[&wait_group](const SeriesWork *series) {
			wait_group.done();
		});

	series->set_context(&url);
	series->start();

	wait_group.wait();
	return 0;
}