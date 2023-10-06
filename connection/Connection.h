#pragma once
#include <functional>
#include <unistd.h>
#include <string.h>
#include <string>
#include <iostream>
#include "../socket/Socket.h"
#include "../channel/Channel.h"
#include "../utils/utils.h"
#include "../buffer/Buffer.h"
#include "../timer/Timer.h"
#include "../log/Log.h"

class Connection
{
public:
    const int DEFAULT_EXPIRED_TIME = 5000;              // ms
    const int DEFAULT_KEEP_ALIVE_TIME = 5 * 60 * 1000;  // ms
    enum State {Invalid = 0, Connecting, Connected, Closed};

private:
    State stateConn;
    TimerNode* timerNode;
    std::unique_ptr<Socket> sock;
    std::unique_ptr<Channel> channel;
    std::unique_ptr<Buffer> readBuf;
    std::unique_ptr<Buffer> sendBuf;

    std::function<void(int, std::string)> deleteConnectionCallback;
    std::function<void(Connection *, int)> onHandleCallback;

    void readNonBlocking();
    void writeNonBlocking();
    void readBlocking();
    void writeBlocking();

public:
    // 初始化
    Connection(EventLoop *_loop, int _fd, sockaddr_in _addr);

    State getStateConn() const;
    void setStateConn(State _state);
    void setTimerNode(TimerNode* _timerNode);
    TimerNode* getTimerNode();
    Socket* getSocket() const;
    void closeConn();
    
    // 设置和获取缓冲区的数据
    void setSendBuf(const char *str);
    void setSendBuf(const std::string& str);
    Buffer* getReadBuf();
    Buffer* getSendBuf();

    // 进行操作、读、写和发送
    void handleConn();
    void readConn();
    void writeConn();

    // 设置与连接相关的回调函数
    void setDeleteConnectionCallback(std::function<void(int, std::string)> const &fn);
    void setOnHandleCallback(std::function<void(Connection *, int)> const &fn);
};

