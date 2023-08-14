#pragma once
#include <functional>
#include <cstdint>
#include <unistd.h>
#include <sys/epoll.h>
#include "../socket/Socket.h"

class EventLoop;
class Channel
{
private:
    // Channel类并不拥有该事件循环资源，仅仅是为了访问而存在的一个指针，所以在该Channel被销毁时，也绝不可以释放loop_，
    // 所以这里使用裸指针来表示仅需访问但不拥有的资源
    EventLoop *loop;
    int fd;
    uint32_t listenEvents;
    uint32_t readyEvents;
    bool inEpoll;

    std::function<void()> readCallback;
    std::function<void()> writeCallback;
public:
    Channel(EventLoop* _loop, int _fd);
    ~Channel();

    void handleEvent();
    void enableVoid();
    void enableRead();
    void enableWrite();
    void useET();

    int getFd() const;
    void setReadyEvent(uint32_t _ev);
    uint32_t getListenEvents();
    uint32_t getReadyEvents();
    bool getInEpoll();
    void setInEpoll(bool _in = true);

    void setReadCallback(std::function<void()> const &callback);
    void setWriteCallback(std::function<void()> const &callback);
};

