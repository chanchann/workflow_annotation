#ifndef BASE64_H
#define BASE64_H

#include <string>

std::string base64_encode(const char* bytes_to_encode, unsigned int in_len);
std::string base64_decode(const std::string& encoded_string);

#endif // BASE64_H