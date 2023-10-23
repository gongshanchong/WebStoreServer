#include "Acceptor.h"

Acceptor::Acceptor(EventLoop* _loop, uint16_t port, const char *ip){
    sock = std::make_unique<Socket>();
    // acceptor使用阻塞式IO比较好
    // Acceptor使用LT模式，建立好连接后处理事件的fd读写用ET模式
    sock->Create();
    // 绑定地址快速重用
    int val = 1;
    if (setsockopt(sock->getFd(), SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) != 0) {
        LOG_ERROR("setsockopt REUSEADDR error, errno=%d, error=%s", errno, strerror(errno));
    }
    sock->bind(port, ip);
    sock->listen();

    channel = std::make_unique<Channel>(_loop, sock->getFd());
    std::function<void()> cb = std::bind(&Acceptor::acceptConnection, this);
    
    channel->setReadCallback(cb);
    channel->enableRead();
}

Acceptor::~Acceptor(){}

void Acceptor::acceptConnection() const{
    int clnt_fd = -1;
    //初始化客户端连接地址
    struct sockaddr_in client_address;
    socklen_t client_addrlength = sizeof(client_address);

    sock->accept(clnt_fd, client_address, client_addrlength);

    // 默认创建socket都是阻塞模式的
    // 新接受到的连接设置为非阻塞式
    setNonBlocking(clnt_fd); 
    if (newConnectionCallback) {
        newConnectionCallback(clnt_fd, client_address);
    }
}

void Acceptor::setNewConnectionCallback(std::function<void(int, sockaddr_in)> const &callback) {
    newConnectionCallback = std::move(callback);
}