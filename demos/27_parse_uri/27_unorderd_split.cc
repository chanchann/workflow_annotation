#include <unordered_map>
#include <string>
#include <vector>
#include <iostream>
#include "workflow/StringUtil.h"

std::unordered_map<std::string, std::string> split_query(const std::string &query)
{
    std::unordered_map<std::string, std::string> res;

    if (query.empty())
        return res;

    std::vector<std::string> arr = StringUtil::split(query, '&');

    if (arr.empty())
        return res;

    for (const auto& ele : arr)
    {
        if (ele.empty())
            continue;

        std::vector<std::string> kv = StringUtil::split(ele, '=');
        size_t kv_size = kv.size();
        std::string& key = kv[0];

        if (key.empty() || res.count(key) > 0)
            continue;

        if (kv_size == 1)
        {
            res.emplace(std::move(key), "");
            continue;
        }

        std::string& val = kv[1];

        if (val.empty())
            res.emplace(std::move(key), "");
        else
            res.emplace(std::move(key), std::move(val));
    }

    return res;
}

int main()
{
    std::string query = "host=chanchan&password=123";
    auto map = split_query(query);
    for(auto& m : map)
    {
        std::cout << m.first << " : " << m.second << std::endl;
    }
    return 0;
}