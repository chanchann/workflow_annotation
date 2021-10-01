#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

// 1. 先创建一个类型为pthread_key_t类型的变量。
// 将pthread_key_t 申明为全局变量
pthread_key_t key; 

struct test_struct { 
    int i;
    float k;
};

void *child1(void *arg)
{
    struct test_struct struct_data; 
    struct_data.i = 10;
    struct_data.k = 3.1415;
    // 每个线程中使用pthread_setspecific来对同一个key设置值
    pthread_setspecific(key, &struct_data); 
    printf("child1 -- address of struct_data is --> 0x%p\n", &(struct_data));
    printf("child1 -- from pthread_getspecific(key) get the pointer and it points to --> 0x%p\n", (struct test_struct *)pthread_getspecific(key));
    printf("child1 -- from pthread_getspecific(key) get the pointer and print it's content:\nstruct_data.i:%d\nstruct_data.k: %f\n", 
        ((struct test_struct *)pthread_getspecific(key))->i, ((struct test_struct *)pthread_getspecific(key))->k); 
    // 每个线程中使用pthread_getspecific来通过同一个key取得值

    printf("------------------------------------------------------\n");
}
void *child2(void *arg)
{
    int temp = 20;
    sleep(2);
    printf("child2 -- temp's address is 0x%p\n", &temp);
    pthread_setspecific(key, &temp); 
    printf("child2 -- from pthread_getspecific(key) get the pointer and it points to --> 0x%p\n", (int *)pthread_getspecific(key));
    printf("child2 -- from pthread_getspecific(key) get the pointer and print it's content --> temp:%d\n", *((int *)pthread_getspecific(key)));
}
int main(void)
{
    pthread_t tid1, tid2;
    
    // int pthread_key_create(pthread_key_t *key,void (*destructor)(void*));
    // 第二个参数是一个清理函数，用来在线程释放该线程存储的时候被调用
    // 该函数指针可以设成 NULL，这样系统将调用默认的清理函数
    pthread_key_create(&key, NULL); 

    pthread_create(&tid1, NULL, child1, NULL);
    pthread_create(&tid2, NULL, child2, NULL);
    
    pthread_join(tid1, NULL);
    pthread_join(tid2, NULL);
    
    pthread_key_delete(key);
    return (0);
}