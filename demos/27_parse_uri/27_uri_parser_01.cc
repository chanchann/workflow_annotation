#include <workflow/URIParser.h>

void test01() 
{
    ParsedURI pu;
    // 这里一定要有完整的url
    URIParser::parse("http://127.0.0.1/d?host=www.baidu.com", pu);
    fprintf(stderr, "path : %s\n", pu.path);
    fprintf(stderr, "query : %s\n", pu.query);
}

std::string url = "http://127.0.0.1:9001/query_list/username=123&password=000";

// "http://127.0.0.1/api/v1/chanchan?host=www.baidu.com&name=chanchann"

void test02()
{
    ParsedURI pu;
    // 这里一定要有完整的url
    if(URIParser::parse(url, pu) < 0)
    {
        fprintf(stderr, "url is invalid ,return ...");
        return;
    }
    // path : /api/v1/chanchan
    fprintf(stderr, "path : %s\n", pu.path);
    auto path_vec = URIParser::split_path(pu.path);
    // api     v1      chanchan
    for(auto& p : path_vec)
    {
        fprintf(stderr, "%s\t", p.c_str());
    }
    fprintf(stderr, "\n");
    // query : host=www.baidu.com
    fprintf(stderr, "query : %s\n", pu.query);
    auto query_map = URIParser::split_query(pu.query);
    for(auto& q : query_map)
    {
        fprintf(stderr, "%s : %s\n", q.first.c_str(), q.second.c_str());
    }
}

void test03()
{
    std::string url = "http://www.sogou.com";
    // std::string url = "http://www.sogou.com/";
    ParsedURI pu;
    if(URIParser::parse(url, pu) < 0)
    {
        fprintf(stderr, "url is invalid ,return ...");
        return;
    }
    fprintf(stderr, "path : %s\n", pu.path);
}

int main() {
    test03();
}