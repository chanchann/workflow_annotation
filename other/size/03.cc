#include <iostream>

void test(size_t a_1) 
{
    int a = a_1;
    if(a < 0)
        std::cout << "less than 0" << std::endl;
    std::cout << a << std::endl;
    int end = 10;
    int start = end + a;
    std::cout << "start : " << start << " end : " << end << std::endl;
}

int main()
{
    test(-12);

}