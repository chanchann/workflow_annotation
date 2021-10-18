#include <workflow/StringUtil.h>
#include <iostream>

void test_01()
{
    auto split_list = StringUtil::split("123,24,21", ',');
    for(auto &s : split_list) 
    {
        std::cout << s << std::endl;
    }
}

void test_02()
{
    auto split_list = StringUtil::split("123", ',');
    for(auto &s : split_list) 
    {
        std::cout << s << std::endl;
    }
}

int main()
{
    // test_01();
    test_02();
}