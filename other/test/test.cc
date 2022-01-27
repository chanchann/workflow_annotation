#include <iostream>
#include <vector>

void test(const std::vector<int> &vec)
{
    for(auto v : vec)
    {
        std::cout << v << std::endl;
    }
}

int main()
{
    test({1,2,3,4});
}