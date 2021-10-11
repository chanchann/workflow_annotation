#include <workflow/URIParser.h>

void test01() {
    ParsedURI pu;
    URIParser::parse("http://127.0.0.1/d?host=www.baidu.com", pu);
    fprintf(stderr, "path : %s\n", pu.path);
    fprintf(stderr, "query : %s\n", pu.query);
}

int main() {
    test01();
}