#ifndef A_H
#define A_H

#include "global.h"

inline void func()
{
    fprintf(stderr, "addr1 : %p\n", &gobal_str);
}



#endif  // A_H