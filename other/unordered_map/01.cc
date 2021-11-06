#include <iostream>
#include <unordered_map>

int main()
{
    std::unordered_map<std::string, std::string> map;
    map["123"] = "abc";
    map["4"] = "d";
    auto var1 = map["123"];
    std::cout << "va1 : " << var1 << std::endl;
    // auto var2 = map.at("unkonw");  // throw exeption
    auto var2 = map["unknow"];
    std::cout << "var2 : " << var2 << std::endl;
    return 0;
}