#pragma once
#include <functional>
#include <memory>
#include <iostream>
#include "../eventloop/EventLoop.h"
#include "../socket/Socket.h"
#include "../channel/Channel.h"
#include "../log/Log.h"

class Acceptor
{
private:
    //智能指针
    std::unique_ptr<Socket> sock;
    std::unique_ptr<Channel> channel;

    std::function<void(int, sockaddr_in)> newConnectionCallback;
public:
    // 指定构造函数或转换函数 (C++11起)为显式, 即它不能用于隐式转换和复制初始化
    explicit Acceptor(EventLoop* _loop, uint16_t port = 3389, const char *ip = nullptr);
    ~Acceptor();

    void acceptConnection() const;
    void setNewConnectionCallback(std::function<void(int, sockaddr_in)> const &callback);
};

