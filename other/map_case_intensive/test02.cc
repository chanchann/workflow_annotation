#include    <map>
#include    <string>
#include    <cstring>
#include    <iostream>

using std::string;
using std::map;
using std::cout;
using std::endl;

 struct ci_less
  {
    // case-independent (ci) compare_less binary function
    struct nocase_compare
    {
      bool operator() (const unsigned char& c1, const unsigned char& c2) const {
          return tolower (c1) < tolower (c2); 
      }
    };
    bool operator() (const std::string & s1, const std::string & s2) const {
      return std::lexicographical_compare 
        (s1.begin (), s1.end (),   // source range
        s2.begin (), s2.end (),   // dest range
        nocase_compare ());  // comparison
    }
  };

typedef map< string, int, ci_less> mapLibc_t;

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