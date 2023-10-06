#pragma once
#include <string>
#include <string.h>
#include <memory>
#include <iostream>

class Buffer
{
private:
    std::string buf;
public:        
    void append(const char* _str, int _size);
    ssize_t size();
    const char* cStr();
    std::string& getBuf();
    void setBuf(const char* _buf);
    void setBuf(const std::string& _buf);
    void clear();
};

