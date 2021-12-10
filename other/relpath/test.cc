#include <limits.h> /* PATH_MAX */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>


int main(void) {
    char buf[PATH_MAX]; /* PATH_MAX incudes the \0 so +1 is not required */
    char *res = realpath("text.txt", buf);
    if (res) { // or: if (res != NULL)123
        printf("This source is at %s\n", buf);
    } else {
        char* errStr = strerror(errno);
        printf("error string: %s\n", errStr);

        perror("realpath");
        exit(EXIT_FAILURE);
    }
    return 0;
}