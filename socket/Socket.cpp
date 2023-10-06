#include "Socket.h"

Socket::Socket() : fd(-1){}

Socket::Socket(int _fd, sockaddr_in _addr): fd(_fd), addr(_addr){    
    errif(fd == -1, "socket create error");
}

Socket::~Socket(){
    if(fd != -1){
        close(fd);
        // close(fd)后fd仍然为原值，需要手动置为-1
        fd = -1;
    }
}

void Socket::Create(){
    fd = socket(AF_INET, SOCK_STREAM, 0);
    errif(fd == -1, "socket create error");
}

void Socket::bind(uint16_t port, const char *ip){
    assert(fd != -1);

    sockaddr_in tempAddr;
    memset(&tempAddr, 0, sizeof(tempAddr));
    tempAddr.sin_family = AF_INET;
    tempAddr.sin_port = htons(port);

    // 根据传入的 ip 确定是否指定 ip 地址
    if(ip != nullptr){
        tempAddr.sin_addr.s_addr = inet_addr(ip);
    }else{
        // INADDR_ANY表示的是一个服务器上所有的网卡（服务器可能不止一个网卡）多个本地ip地址都进行绑定端口号，进行侦听
        // 就是指定地址为0.0.0.0的地址,这个地址事实上表示不确定地址,或“所有地址”、“任意地址”
        tempAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    }

    errif(::bind(fd, (sockaddr*)&tempAddr, sizeof(tempAddr)) == -1, "socket bind error");

    this->setAddr(tempAddr);
}

void Socket::listen() const{
    assert(fd != -1);
    errif(::listen(fd, SOMAXCONN) == -1, "socket listen error");
}

void Socket::accept(int &clnt_fd, sockaddr_in &client_address, socklen_t &client_addrlength) const{
    assert(fd != -1);

    clnt_fd = ::accept(fd, (sockaddr*)&client_address, &client_addrlength);
    errif(clnt_fd == -1, "Failed to accept socket");

}

int Socket::getFd() const{
    return fd;
}

std::string Socket::getAddr() const{
    std::string ret(inet_ntoa(addr.sin_addr));
    ret += ":";
    ret += std::to_string(htons(addr.sin_port));
    
    return ret;
}

void Socket::setFd(int _fd){ fd = _fd; }

void Socket::setAddr(sockaddr_in _addr){ addr = _addr; }
