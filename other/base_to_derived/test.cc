#include <iostream>

class Base {
public:
    Base() : b(new int) {};
    ~Base() { delete b; };
    Base(Base&& other) 
        : a(other.a) 
    { b = other.b; other.b = nullptr; }

    Base &operator=(Base &&other) 
    { a = other.a; b = other.b; other.b = nullptr; return *this; }

    int a = 0;
    int *b;
};

class Derived : public Base
{
public:
    Derived() : c(new int) {};
    ~Derived() { delete c; };
    Derived(Derived&& other) 
    { c = other.c; other.c = nullptr; }

    Derived &operator=(Derived &&other) 
    { c = other.c; other.c = nullptr;  return *this; }

    int *c;
};

int main()
{
    Base *base = new Base();
    // Derived *derived = std::move(static_cast<Derived *>(base));
    Derived *derived = new Derived;
    *derived = std::move(*static_cast<Derived *>(base));
    derived->c;
}