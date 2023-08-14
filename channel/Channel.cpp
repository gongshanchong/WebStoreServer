#include "Channel.h"
#include "../eventloop/EventLoop.h"

Channel::Channel(EventLoop* _loop, int _fd) 
    : loop(_loop), fd(_fd), listenEvents(0), readyEvents(0), inEpoll(false){}

Channel::~Channel(){ loop->deleteChannel(this); }

void Channel::handleEvent(){
    if(readyEvents & (EPOLLIN | EPOLLPRI)){
        readCallback();
    }
    // 当写缓冲区不为空时触发写事件
    // 检测到可以写的时候，写缓冲区没有满进行触发
    // 应用程序调用epoll_wait，当该socket 的写缓冲有空余时，就返回对应的写事件，应用程序这时候就可以调用发送数据
    if(readyEvents & (EPOLLOUT)){
        writeCallback();
    }
}

void Channel::enableVoid(){
    listenEvents = 0;
    loop -> updateChannel(this);
}

void Channel::enableRead(){
    listenEvents |= EPOLLIN | EPOLLPRI;
    loop -> updateChannel(this);
}

void Channel::enableWrite(){
    listenEvents |= EPOLLOUT;
    loop -> updateChannel(this);
}

void Channel::useET(){
    listenEvents |= EPOLLET;
    loop->updateChannel(this);
}

int Channel::getFd() const{
    return fd;
}

void Channel::setReadyEvent(uint32_t _ev){
    if (_ev & EPOLLIN) {
        readyEvents |= EPOLLIN;
    }
    if (_ev & EPOLLOUT) {
        readyEvents |= EPOLLOUT;
    }
    if (_ev & EPOLLET) {
        readyEvents |= EPOLLET;
    }
}

uint32_t Channel::getListenEvents(){
    return listenEvents;
}
uint32_t Channel::getReadyEvents(){
    return readyEvents;
}

bool Channel::getInEpoll(){
    return inEpoll;
}

void Channel::setInEpoll(bool _in){
    inEpoll = _in;
}

void Channel::setReadCallback(std::function<void()> const &callback) { 
    readCallback = std::move(callback); 
}

void Channel::setWriteCallback(std::function<void()> const &callback) { 
    writeCallback = std::move(callback); 
}
