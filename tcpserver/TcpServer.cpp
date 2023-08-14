#include "TcpServer.h"
#include <iostream>

TcpServer::TcpServer(uint16_t port, const char *ip, std::string _logFileName, int _maxLines): logFileName(_logFileName), maxLines(_maxLines){ 
    // Acceptor、定时器容器由且只由mainReactor负责
    // 使用one loop per thread模式
    mainReactor = std::make_unique<EventLoop>();
    acceptor = std::make_unique<Acceptor>(mainReactor.get(), port, ip);
    std::function<void(int, sockaddr_in)> cb = std::bind(&TcpServer::newConnection, this, std::placeholders::_1, std::placeholders::_2);
    acceptor->setNewConnectionCallback(cb);

    // 定时器
    // 只有mainReactor有定时器容器，管理所有的链接定时器
    timerManager = std::make_unique<TimerManager>(mainReactor.get());

    // 线程数量，也是subReactor数量
    unsigned int size = std::thread::hardware_concurrency();
    // 新建线程池
    thpool = std::make_unique<ThreadPool>(size);
    for(int i = 0; i < size; ++i){
        // 每一个线程是一个EventLoop
        std::unique_ptr<EventLoop> sub_reactor = std::make_unique<EventLoop>();
        subReactors.push_back(std::move(sub_reactor));
    }

    // 初始化日志，文件名（最后/前的为目录路径，最后/后的为文件名）
    // "./serverLog/serverLog", 5000000
    Log::getInstance()->init(logFileName.c_str(), maxLines);
}

TcpServer::~TcpServer(){}

void TcpServer::Start(){
    int len = subReactors.size();
    for(size_t i = 0; i < len; ++i){
        // 每个subReactor都执行loop函数
        std::function<void()> sub_loop = std::bind(&EventLoop::loop, subReactors[i].get());
        thpool->add(std::move(sub_loop));
    }
    // 设置信号传送闹钟，即用来设置信号SIGALRM在经过参数seconds秒数后发送给目前的进程
    // alarm定时触发SIGALRM信号，epoll监听到此信号进而进行处理
    alarm(DEFAULT_EXPIRED_TIME / 1000);
    //  主分发器进行监听
    mainReactor->loop();
}

void TcpServer::newConnection(int sockfd, sockaddr_in sockaddr){
    errif(sockfd == -1, "socket connect error");
    errif(subReactors.size() == 0, "thread reactor error");

    // 调度策略：全随机
    int random = sockfd % subReactors.size();
    // 分配给一个subReactor
    std::unique_ptr<Connection> conn = std::make_unique<Connection>(subReactors[random].get(), sockfd, sockaddr);
    std::function<void(int, std::string)> cb = std::bind(&TcpServer::deleteConnection, this, std::placeholders::_1, std::placeholders::_2);
    conn->setDeleteConnectionCallback(cb);
    conn->setOnHandleCallback(onHandleCallback);

    // 生成新的定时器并加入到定时器容器中
    std::shared_ptr<TimerNode> newNode = std::make_shared<TimerNode>(sockfd, conn->getSocket()->getAddr(),4 * DEFAULT_EXPIRED_TIME);
    // 每一个定时器里面放置一个回调函数，用来关闭过期连接
    newNode->setTimeoutCallBack(cb);
    // 为链接绑定定时器
    // 当最后一个对应的智能指针被销毁时，get() 返回的指针就无效了
    // 若智能指针释放了其对象，返回的指针所指向的对象也就消失了
    // get 用来将指针的访问权限传递给代码
    conn->setTimerNode(newNode.get());
    // std::move() 效率更高，因为移动不涉及引用计数的增减，移动后原来的智能指针的属性（计数）转移到新的变量上来了，原来的智能指针计数变为0
    timerManager->addTimer(std::move(newNode));

    // 日志输出 
    LOG_INFO("New connection client(%s)", (conn->getSocket()->getAddr() + " fd of " + std::to_string(sockfd)).c_str());

    // 添加到链接组中
    connections[sockfd] = std::move(conn);
}

void TcpServer::deleteConnection(int sockfd,  std::string sockaddr){
    if((sockfd == -1) || (connections.find(sockfd) == connections.end())) return;

    auto it = connections.find(sockfd);
    // 如果是超时前关闭，则将定时器容器中定时器设置为无效
    if(! it->second->getTimerNode()->getDeleted()) 
        it->second->getTimerNode()->setDeleted();
    // 日志输出
    else
        LOG_INFO("client %s close of timeout", (sockaddr + " fd of " + std::to_string(sockfd)).c_str());

    // 智能指针conn，当conn引用计数为0时，conn会销毁，conn中的智能指针sock、channel也会销毁
    it->second->setStateConn(Connection::Closed);
    connections.erase(sockfd);
    LOG_INFO("client %s close", (sockaddr + " fd of " + std::to_string(sockfd)).c_str());
    Log::getInstance()->flush();
}

// 自定义业务逻辑
// 以回调函数的方式编写业务逻辑
void TcpServer::setOnHandle(std::function<void(Connection *, int)> const &fn){
    // std::move() 函数将一个左值强制转化为右值引用，以用于移动语义
    // 移动语义，允许直接转移对象的资产和属性的所有权，而在参数为右值时无需复制它们
    // std::move() 将对象的状态或者所有权从一个对象转移到另一个对象，只是转移，没有内存的搬迁或者内存拷贝
    onHandleCallback = std::move(fn);
}
