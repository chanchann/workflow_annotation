#include <iostream>

void test(size_t a) 
{
    std::cout << a << std::endl;
    size_t end = 10;
    size_t start = end + a;
    std::cout << "start : " << start << " end : " << end << std::endl;
}

int main()
{
    test(-12);

}