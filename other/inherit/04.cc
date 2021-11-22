#include <iostream> 
using namespace std; 

class Base { 
public: 
    void g(float x) { 
        cout << "Base::g(float) " << x << endl; 
    } 
}; 

class Derived : public Base { 
private: 
    void g(int x) { 
        cout << "Derived::g(int) " << x << endl; 
    } 
}; 

int main(void) 
{ 
    Derived d; 
    Base *pb = &d; 
    Derived *pd = &d; 

    pb->g(3.14f); 
    // pd->g(3); 
} 