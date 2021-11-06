#include <workflow/URIParser.h>

void test01() {
    const char *request_uri = "/d?host=www.baidu.com";
    const char *p = request_uri;

    printf("Request-URI: %s\n", request_uri);
    while (*p && *p != '?')
        p++;

    std::string path(request_uri, p - request_uri);
    printf("path: %s\n", path.c_str());

    std::string query(p+1);
    printf("query: %s\n", query.c_str());
}

// void test02() 
// {

// }

int main() {
    test01();
}