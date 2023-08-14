#pragma once
#include <arpa/inet.h>
#include <unistd.h>
#include <assert.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <string>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "../utils/utils.h"

class Socket
{
private:
    int fd;
    sockaddr_in addr;
public:
    Socket();
    Socket(int _fd, sockaddr_in _addr);
    ~Socket();
    void Create();

    void bind(uint16_t port, const char *ip = nullptr);
    void listen() const;
    void accept(int &clnt_fd, sockaddr_in &client_address, socklen_t &client_addrlength) const;
    
    int getFd() const;
    std::string  getAddr() const;
    void setFd(int fd);
    void setAddr(sockaddr_in _addr);
};

