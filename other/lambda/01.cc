#include <iostream>
using namespace std;

int main()
{
    int *a = new int(10);
    auto f = [&a] {
        cout << *a << endl;  
        delete a;
    }; 
    double *b = new double(2.0);
    auto f1 = [=] {
        cout << *b << endl;  
        delete b;
    }; 
    double *c = new double(3.0);
    auto f2 = [c] {
        cout << *c << endl;  
        delete c;
    }; 
    cout << "test" << endl;
    f(); 
    f1();
    f2();
}