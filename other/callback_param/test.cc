#include <functional>

// stack overflow but can't compile
struct Call
{
    template<typename F>
    explicit Call(F f) : zero_(f), one_(std::move(f)) {}

    void operator()() { zero_(); }
    void operator()(int i) { one_(i); }

    std::function<void()>    zero_;
    std::function<void(int)> one_;
};

int main()
{
    Call c1([]() { fprintf(stderr, "no param\n"); });
    // c1();

    // Call c2([](int i) { fprintf(stderr, "%d\n", i); });
    // c2(1);
}