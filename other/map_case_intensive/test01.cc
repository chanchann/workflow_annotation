#include    <map>
#include    <string>
#include    <cstring>
#include    <iostream>

using std::string;
using std::map;
using std::cout;
using std::endl;

// recommended in Meyers, Effective STL when internationalization and embedded
// NULLs aren't an issue.  Much faster than the STL or Boost lex versions.
struct ciLessLibC : public std::binary_function<string, string, bool> {
    bool operator()(const string &lhs, const string &rhs) const {
        return strcasecmp(lhs.c_str(), rhs.c_str()) < 0 ;
    }
};

typedef map< string, int, ciLessLibC> mapLibc_t;

int main(void) {
    mapLibc_t cisMap; // change to test other comparitor 

    cisMap["foo"] = 1;
    cisMap["FOO"] = 2;

    cisMap["bar"] = 3;
    cisMap["BAR"] = 4;

    cisMap["baz"] = 5;
    cisMap["BAZ"] = 6;

    cout << "foo == " << cisMap["foo"] << endl;
    cout << "bar == " << cisMap["bar"] << endl;
    cout << "baz == " << cisMap["baz"] << endl;

    if(cisMap.count("FoO")) cout << "has foo" << endl;

    return 0;
}