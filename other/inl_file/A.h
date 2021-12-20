#ifndef A_H
#define A_H

class A
{
public:
    template<typename T>
    void test(T t);
};

#include "A.inl"

#endif // A_H