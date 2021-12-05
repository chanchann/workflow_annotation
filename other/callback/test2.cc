#include <functional>
#include <iostream>

using Handler = std::function<void(int)>;
using SeriesHandler = std::function<void(int, int)>;

struct A 
{
    Handler handler;
    SeriesHandler series_handler;
};

void foo_a(int i)
{
    std::cout << i << std::endl;
}

void foo_b(int i, int j)
{
    std::cout << i << " " << j << std::endl;
}

int main()
{
    A a;
    a.handler = foo_a;
    a.handler(1);

    A a1;
    a1.series_handler = foo_b;
    if(foo_b)
    {
        foo_b(1, 2);
    }

}