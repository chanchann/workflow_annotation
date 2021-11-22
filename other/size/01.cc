#include <iostream>

void test(size_t a) 
{
    std::cout << a << std::endl;
    if(a == -1)
    {
        std::cout << "-1" << std::endl;
    } else if(a == -3)
    {
        std::cout << "-3" << std::endl;
    }
    int end = 10;
    int start = end + a;
    std::cout << "start : " << start << std::endl;
    
}

int main()
{
    test(1);
    test(-1);    
    test(-3);
}