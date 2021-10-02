/*
https://stackoverflow.com/questions/1228025/pthread-key-t-and-pthread-once-t

pthread_key_t is for creating thread thread-local storage: each thread gets its own copy of a data variable, instead of all threads sharing a global (or function-static, class-static) variable. 

线程局部存储

对于一个存在多个线程的进程来说，有时候我们需要有一份数据是每个线程都拥有的

我们把这样的数据称之为线程局部存储（Thread Local Storage，TLS），对应的存储区域叫做线程局部存储区。

接口 :

int pthread_key_create(pthread_key_t* key, void (*destructor)(void*));
int pthread_key_delete(pthread_key_t key);

int pthread_setspecific(pthread_key_t key, const void* value);
void* pthread_getspecific(pthread_key_t key);

pthread_key_create 函数调用成功会返回 0 值，调用失败返回非 0 值，函数调用成功会为线程局部存储创建一个新键，

用户通过参数 key 去设置（调用 pthread_setspecific）和获取（pthread_getspecific）数据，

因为进程中的所有线程都可以使用返回的键，所以参数 key 应该指向一个全局变量。

参数 destructor 是一个自定义函数指针，其签名是：

void* destructor(void* value)
{
    多是为了释放 value 指针指向的资源
}

*/


#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

//线程局部存储 key
pthread_key_t thread_log_key;

void write_to_thread_log(const char* message)
{
    if (message == NULL)
        return;

    FILE* logfile = (FILE*)pthread_getspecific(thread_log_key);
    fprintf(logfile, "%s\n", message);
    fflush(logfile);
} 

void close_thread_log(void* logfile)
{
    char logfilename[128];
    sprintf(logfilename, "close logfile: thread%ld.log\n", (unsigned long)pthread_self());
    printf("%s", logfilename);

    fclose((FILE *)logfile);
} 

void* thread_function(void* args)
{
    char logfilename[128];
    sprintf(logfilename, "thread%ld.log", (unsigned long)pthread_self());

    FILE* logfile = fopen(logfilename, "w");
    if (logfile != NULL)
    {
        pthread_setspecific(thread_log_key, logfile);

        write_to_thread_log("Thread starting...");
    }

    return NULL;
} 

int main()
{
    pthread_t threadIDs[5]; 
    // pthread_key_create 函数来申请一个槽位。在 NPTL 实现下，pthread_key_t 是无符号整型
    // pthread_key_create 调用成功时会将一个小于 1024 的值填入第一个入参指向的 pthread_key_t 类型的变量中
    
    pthread_key_create(&thread_log_key, close_thread_log);
    for(int i = 0; i < 5; ++i)
    {
        pthread_create(&threadIDs[i], NULL, thread_function, NULL);
    }

    for(int i = 0; i < 5; ++i)
    {
        pthread_join(threadIDs[i], NULL);
    }

    return 0;
}