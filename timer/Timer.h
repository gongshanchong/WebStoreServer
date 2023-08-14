#pragma once
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <deque>
#include <memory>
#include <queue>
#include "../eventloop/EventLoop.h"
#include "../log/Log.h"

const int DEFAULT_SENCOND_TIME = 1000;              // ms
const int DEFAULT_EXPIRED_TIME = 5000;              // ms
const int DEFAULT_KEEP_ALIVE_TIME = 5 * 60 * 1000;  // ms

// 定时器
class TimerNode 
{
private:
    // 定时器对应链接的文件描述符
    int sockfd;
    // 链接对应的地址
    std::string sockaddr;
    // 是否过期
    bool deleted;
    // 保留时间(毫秒)
    size_t expiredTime;

public:
    TimerNode(int _fd, std::string _sockaddr, int timeout);
    ~TimerNode();
    
    void update(int timeout);
    bool isValid();
    void setDeleted(){ deleted = true; }
    bool getDeleted() const { return deleted; }
    size_t getExpTime() const { return expiredTime; }
    int getFd() const { return sockfd; }
    std::string getAddr() const {return sockaddr; }

    // 我们给每一个定时器里面放置一个回调函数，用来关闭过期连接
    std::function<void(int, std::string)> timeoutCallBack;
    void setTimeoutCallBack(std::function<void(int, std::string)> const &callback);
};

struct TimerCmp {
  // 按引用传参不增加引用计数
  bool operator()(std::shared_ptr<TimerNode> &a, std::shared_ptr<TimerNode> &b) const {
    return a->getExpTime() > b->getExpTime();
  }
};

// 定时器容器
class TimerQueue{
private:
    // shared_ptr被销毁（如离开作用域）引用数减一
    typedef std::shared_ptr<TimerNode> sptTimerNode;
    std::priority_queue<sptTimerNode, std::deque<sptTimerNode>, TimerCmp> timerNodeQueue;

public:
    TimerQueue();
    ~TimerQueue();

    void addTimer(std::shared_ptr<TimerNode> node);
    void handleExpiredEvent();
};

// 定时事管理
class TimerManager{
private:
    static int pipefd[2];
    std::unique_ptr<Channel> channel;
    std::unique_ptr<TimerQueue> timerQueue;

    //信号处理函数
    static void sigHandle(int sig);
    //设置信号函数
    void addSig(int sig, void(handler)(int), bool restart = true);

public:
    // 指定构造函数或转换函数 (C++11起)为显式, 即它不能用于隐式转换和复制初始化
    explicit TimerManager(EventLoop* _loop);
    ~TimerManager();

    void addTimer(std::shared_ptr<TimerNode> node);
    void handleExpiredEvent();
};
