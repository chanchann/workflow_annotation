#include <iostream> 
using namespace std; 

class Base { 
public: 
    void f(int a){ 
        cout << "Base::f(int a)" << endl; 
    } 
    virtual void g(int a) { 
        cout << "virtual Base::g(int a)" << endl; 
    } 
}; 

class Derived : public Base 
{ 
public: 
    void h(int a) { 
        cout << "Derivd::h(int a)" << endl; 
    } 
    virtual void g(int a) { 
        cout << "virtual Derived::g(int a)" << endl; 
    } 
}; 

int main() 
{ 
    Base b; 
    b.f(3); 
    b.g(4); 

    Derived d; 
    d.f(3); 
    d.g(4); 
    d.h(3); 
} 
