#include <iostream>
#include "A.h"

template<typename T>
void A::test(T t)
{
    std::cout << t << std::endl;
}

class B
{
public:
    template<typename T>
    void test(T t)
    {
        std::cout << t << std::endl;
    }
};
