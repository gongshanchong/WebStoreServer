#include "Buffer.h"

// 由于在处理文件时，里面可能有 \0，所以使用append将所有字符保存到缓冲区
void Buffer::append(const char* _str, int _size){
    buf.append(_str, _size);
}

ssize_t Buffer::size(){
    return buf.size();
}

const char* Buffer::cStr(){
    return buf.c_str();
}

std::string& Buffer::getBuf(){
    return buf;
}

void Buffer::setBuf(const char* _buf){
    int len = strlen(_buf);

    buf.clear();
    append(_buf, len);
}

void Buffer::setBuf(const std::string& _buf){
    buf.clear();
    buf = _buf;
}

void Buffer::clear(){
    buf.clear();
}

