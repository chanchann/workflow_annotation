#include <functional>

void test()
{}
typeded void(*next)() {}

template <typename Signature>
std::function<Signature> cast(void* f)
{
    return static_cast<Signature*>(f);
}

int main()
{
    std::function<void()> f = cast<void()>(&test);
    return 0;
}