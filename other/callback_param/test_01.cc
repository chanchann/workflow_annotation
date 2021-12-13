#include <functional>
#include <iostream>

using Handler = std::function<void(int, int)>;
using SeriesHandler = std::function<void(int, int, int)>;

class Foo
{
public: 
    void handle(const Handler &handler)
    {
        handler_ = [&] (int i, int j, int k) { handler(i, j); };
    }

    // void handle(const Handler &handler, int compute_id)
    // {
    //     handler_ = [&] (int i, int j, int k) { 
    //         WFGoTask *go_task = WFTaskFactory::create_go_task(
    //                                 "wfrest" + std::to_string(compute_id),
    //                                 handler,
    //                                 i, j);
    //         return go_task;
    //     };
    // }
    
    void handle(const SeriesHandler &handler)
    {
        handler_ = handler;
    }
    
    void operator()(int i, int j, int k) { handler_(i, j, k); }
private:
    SeriesHandler handler_;
};


int main()
{
    Foo foo1;
    foo1.handle([](int i, int j) { std::cout << "i : " << i << " j : " << j << std::endl;  });
    foo1(1, 2, 3);

    Foo foo2;
    foo2.handle([](int i, int j, int k) { std::cout << "i : " << i << " j : " << j << " k : " << k << std::endl; });
    foo2(1, 2, 3);
}