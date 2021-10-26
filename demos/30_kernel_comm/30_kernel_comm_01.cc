/*
https://github.com/sogou/workflow/issues/200
*/

// todo ： unifished 

#include <workflow/Communicator.h>
#include <workflow/HttpMessage.h>
#include <workflow/WFFacilities.h>
#include <signal.h>
#include <sys/socket.h>
#include <arpa/inet.h>

class MyHttpSession : public CommSession
{
public: 
    ~MyHttpSession() 
    {
        delete this->get_message_out();
        delete this->get_message_in();
        delete this;
    }
private:
    CommMessageOut *message_out() override { return new protocol::HttpRequest; }
    CommMessageIn *message_in() override { return new protocol::HttpResponse; }
    void handle(int state, int error) override
    {
        protocol::HttpResponse *resp = (protocol::HttpResponse *)this->get_message_in();
        // print resp
        // const void *body;
        // size_t body_len;
        // resp->get_parsed_body(&body, &body_len);

        // fprintf(stderr, "body : %s\n", static_cast<const char *>(body));
        // fprintf(stderr, "resp : %s\n", resp->get_status_code());
        fprintf(stderr, "123");
    }
private:
	int state;
	int error;
};

static WFFacilities::WaitGroup wait_group(1);

void sig_handler(int signo)
{
    wait_group.done();
}

int main()
{
    signal(SIGINT, sig_handler);
    // 1. 先创建poller线程池和hadler线程池
    Communicator comm;
    comm.init(2, 10);
    
    struct sockaddr_in serv_addr;  // 服务器地址结构
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serv_addr.sin_port = htons(8888);

    // 通信目标
    CommTarget target;
    target.init(reinterpret_cast<const struct sockaddr *>(&serv_addr), sizeof serv_addr, 10, 100);
    MyHttpSession* my_session = new MyHttpSession;
    comm.request(my_session, &target);
    wait_group.wait();
    // ...
    delete my_session;
}
