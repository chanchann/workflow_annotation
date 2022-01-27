#include "json.hpp"
#include <iostream>
using Json = nlohmann::json;

struct table
{
    int row_num;
    int col_num;

};

int main()
{
    Json json1;
    Json json2 = {1 , 2, 3};
    json1["1"] = json2;
    json2.push_back("4");
    json1["2"] = json2;
    std::cout << json1.dump() << std::endl;
    
        
}