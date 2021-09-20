/***************************************************************************
 * 
 * Copyright (c) 2014 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
 

#ifndef  __ABASE64_H_
#define  __ABASE64_H_

#include <string.h>
#include <iostream>


std::string base64_encode(unsigned char const* , unsigned int len);
std::string base64_decode(std::string const& s);


#endif  //__BASE64_H_