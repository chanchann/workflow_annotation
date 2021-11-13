#include <workflow/WFTaskFactory.h>
#include <workflow/Workflow.h>
#include <workflow/WFFacilities.h>
#include <sys/stat.h>
#include <csignal>
#include <workflow/HttpMessage.h>

using namespace protocol;

// in server can respond to client we save the file
// void pwrite_callback(WFFileIOTask *task)
// {
//     long ret = task->get_retval();
//     HttpResponse *resp = (HttpResponse *)task->user_data;

//     if (task->get_state() != WFT_STATE_SUCCESS || ret < 0)
//     {
//         resp->set_status_code("503");
//         resp->append_output_body("<html>503 Internal Server Error.</html>\r\n");
//     }
//     else
//     {
//         resp->set_status_code("200");
//         resp->append_output_body("<html>200 success.</html>\r\n");
//     }
// }

void pwrite_callback(WFFileIOTask *task)
{
    fprintf(stderr, "write finish");
}

static WFFacilities::WaitGroup wait_group(1);

void sig_handler(int signo)
{
    wait_group.done();
}

int main()
{
    signal(SIGINT, sig_handler);

    std::string content = "111111111111112222222222222222223333333333330";
    
    std::string path = "./demo.txt";

    WFFileIOTask *pwrite_task = WFTaskFactory::create_pwrite_task(path, 
                                                            static_cast<const void *>(content.c_str()), 
                                                            content.size(), 
                                                            0,
                                                            pwrite_callback);

    pwrite_task->start();
    
    wait_group.wait();                                           
}
