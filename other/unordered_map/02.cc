#include <iostream>
#include <unordered_map>

int main()
{
    std::unordered_map<std::string, std::string> map;
    map["123"] = "abc";
    map["4"] = "d";

    std::unordered_map<std::string, std::string> map2;

    map2 = std::move(map);

    for(auto &m : map2)
    {
        std::cout << m.first << " : " << m.second << std::endl;
    }
    return 0;
}