#include "msgqueue.h"
#include <stdio.h>
#include <string.h>

// https://github.com/sogou/workflow/issues/353


// wrong way:
// int main(int argc, char* argv[]) {
//   msgqueue_t* tmp_mq = msgqueue_create(10, 0);
//   char str[] = "hello";

//   msgqueue_put(str, tmp_mq);
//   char* p = msgqueue_get(tmp_mq);
//   printf("%s\n", p);
//   printf("%c\n", *p++);

//   return 0;
// }


// because :
// The second argument of msgqueue_create() is the offset from the head each message, 
// where users have to reserve bytes of a pointer size for internal linking. 
// This will reduce the allocating and freeing of memory.

int main()
{
    msgqueue_t *mq = msgqueue_create(10, -static_cast<int>(sizeof (void *)));
    char str[sizeof (void *) + 6];
    char *p = str + sizeof (void *);
    strcpy(p, "hello");
    msgqueue_put(p, mq);
    p = static_cast<char *>(msgqueue_get(mq));
    printf("%s\n", p);
    return 0;
}