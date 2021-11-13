## multipart-parser-c using in wfrest

https://github.com/iafonov/multipart-parser-c

https://github.com/chanchann/wfrest/blob/main/src/HttpContent.h

## parse_multipart

我们从核心接口出发，看如何parse_multipart data的

```cpp
std::string boundary = "--" + boundary_;
multipart_parser *parser = multipart_parser_init(boundary.c_str(), &settings_);
```

我们header中给出的boudary 前面加上 "--" 作为下面part的boundary

