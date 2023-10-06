#pragma once
#include <map>
#include <unordered_map>
#include <vector>
#include <unistd.h>
#include <functional>
#include "../socket/Socket.h"
#include "../acceptor/Acceptor.h"
#include "../connection/Connection.h"
#include "../CGImysql/MysqlConnPool.h"
#include "../threadpool/ThreadPool.h"
#include "../eventloop/EventLoop.h"
#include "../utils/utils.h"

class TcpServer
{
private:
    const int DEFAULT_SENCOND_TIME = 1000;              // ms
    const int DEFAULT_EXPIRED_TIME = 5000;              // ms
    const int DEFAULT_KEEP_ALIVE_TIME = 5 * 60 * 1000;  // ms

    // 限制服务器的最大并发连接数
    static const int MAXFDS = 100000;

    // 只负责接受连接，然后分发给一个subReactor
    std::unique_ptr<EventLoop> mainReactor;
    // 连接接受器
    std::unique_ptr<Acceptor> acceptor;
    // TCP连接
    std::unordered_map<int, std::unique_ptr<Connection>> connections;
    // 定时器事件
    std::unique_ptr<TimerManager> timerManager;
    // 线程池
    std::unique_ptr<ThreadPool> thpool;
    // 负责处理事件循环
    std::vector<std::unique_ptr<EventLoop>> subReactors;

    // 自定义业务逻辑
    std::function<void(Connection *, int)> onHandleCallback;
    
    // 一些参数
    // 日志相关
    std::string logFileName;                 // 日志存储的文件名
    int maxLines;                            // 日志

public:
    TcpServer(uint16_t port, const char *ip, std::string _logFileName, int _maxLines);
    ~TcpServer();

    void Start();
    void newConnection(int sockfd, sockaddr_in sockaddr);
    void deleteConnection(int sockfd, std::string sockaddr);

    // 自定义业务逻辑
    // 以回调函数的方式编写业务逻辑
    // 通过设置OnMessage回调函数来自定义自己的业务逻辑，在服务器完全接收到客户端的数据之后，该函数触发
    // 现在Connection类只有从socket读写数据的逻辑，与具体业务没有任何关系，业务完全由用户自定义
    void setOnHandle(std::function<void(Connection *, int)> const &fn);
};


