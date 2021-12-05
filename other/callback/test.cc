#include <functional>
#include <iostream>

using Handler = std::function<void()>;

struct A 
{
    Handler handler;
};


int main()
{
    A a;
    if(a.handler) std::cout << "1" << std::endl;
    else std::cout << "2" << std::endl;
}