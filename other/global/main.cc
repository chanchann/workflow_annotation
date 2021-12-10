#include "global.h"
#include "a.h"

int main()
{
    func();
    fprintf(stderr, "addr2 : %p\n", &gobal_str);
}