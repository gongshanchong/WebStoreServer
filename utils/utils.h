#ifndef UTIL_H
#define UTIL_H

void errif(bool, const char*);

// 设置为非阻塞
void setNonBlocking(int argFd);
bool IsNonBlocking(int argFd);

// 设置优雅关闭连接
void setSocketNoLinger(int fd);

#endif