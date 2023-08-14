#include "Connection.h"

Connection::Connection(EventLoop *_loop, int _sockfd, sockaddr_in _addr){
    sock = std::make_unique<Socket>(_sockfd, _addr);

    if (_loop != nullptr) {
        channel = std::make_unique<Channel>(_loop, _sockfd);
        channel->enableRead();
        // Acceptor使用LT模式，建立好连接后处理事件的fd读写用ET模式
        channel->useET();
    }
    readBuf = std::make_unique<Buffer>();
    sendBuf = std::make_unique<Buffer>();

    stateConn = State::Connected;
    timerNode = nullptr;
}

Connection::~Connection(){}

Connection::State Connection::getStateConn() const{
    return stateConn;
}

void Connection::setStateConn(Connection::State _state){
    stateConn = _state;
}

void Connection::setTimerNode(TimerNode* _timerNode){
    timerNode = _timerNode;
}

TimerNode* Connection::getTimerNode(){
    return timerNode;
}

Socket* Connection::getSocket() const{
    return sock.get();
}

void Connection::closeConn(){
    stateConn = State::Closed;
    
    //关闭socket会自动将文件描述符从epoll树上移除
    deleteConnectionCallback(sock->getFd(), sock->getAddr());
}

void Connection::setSendBuf(const char *str){
    sendBuf->setBuf(str);
}

void Connection::setSendBuf(const std::string& str){
    sendBuf->setBuf(str);
}

Buffer* Connection::getReadBuf(){
    return readBuf.get();
}

Buffer* Connection::getSendBuf(){
    return sendBuf.get();
}

void Connection::handleConn(){
    // 读取数据
    this->readConn();

    // 业务处理逻辑
    onHandleCallback(this, sock->getFd());

    // 发送数据
    this->writeConn();
    
    // 此处更新时间
    // 每次操作后添加30秒
    this->getTimerNode()->update(6 * DEFAULT_EXPIRED_TIME);
    LOG_INFO("%s", "update timer once\r\n");
    Log::getInstance()->flush();
}

void Connection::readConn(){
    if(stateConn != State::Connected) return;

    int sockFd = sock->getFd();
    std::string sockAddr = sock->getAddr();

    readBuf->clear();
    if (IsNonBlocking(sockFd)) {
        this->readNonBlocking();
    } else {
        this->readBlocking();
    }
    
    LOG_INFO("read data from the client(%s)", (sockAddr + " fd of " + std::to_string(sockFd)).c_str());
    // 此处依据接收的文件大小更新时间
    this->getTimerNode()->update((readBuf->size()/1024/100) * DEFAULT_SENCOND_TIME);
    LOG_INFO("%s", "update timer once\r\n");
}

void Connection::writeConn(){
    if(stateConn != State::Connected) return;

    int sockFd = sock->getFd();
    int len = sendBuf->size();
    std::string sockAddr = sock->getAddr();
    LOG_INFO("send data to the client(%s)", (sockAddr + " fd of " + std::to_string(sockFd)).c_str());

    // 此处依据发送的文件大小更新时间
    this->getTimerNode()->update((len/1024/100) * DEFAULT_SENCOND_TIME);
    LOG_INFO("%s", "update timer once\r\n");
    
    if (IsNonBlocking(sockFd)) {
        this->writeNonBlocking();
    } else {
        this->writeBlocking();
    }
    // 清空缓冲区
    sendBuf->clear();
}

void Connection::setDeleteConnectionCallback(std::function<void(int, std::string)> const &fn){
    deleteConnectionCallback = std::move(fn);
}

void Connection::setOnHandleCallback(std::function<void(Connection *, int)> const &fn) {
  onHandleCallback = fn;
  std::function<void()> handle = std::bind(&Connection::handleConn, this);
  channel->setReadCallback(handle);
}

void Connection::readNonBlocking(){
    int sockFd = sock->getFd();
    char buf[2048]; 
    // 使用非阻塞IO，读取客户端buffer，一次读取buf大小数据，直到全部读取完毕
    while (true) {   
        memset(buf, 0, sizeof(buf));
        ssize_t bytes_read = read(sockFd, buf, sizeof(buf));
        if (bytes_read > 0) {
            readBuf->append(buf, bytes_read);
        // 程序正常中断、继续读取
        // 如果进程在一个慢系统调用(slow system call)中阻塞时，当捕获到某个信号且相应信号处理函数返回时，这个系统调用不再阻塞而是被中断，就会调用返回错误（一般为-1）&&设置errno为EINTR
        } else if (bytes_read == -1 && errno == EINTR) {
            LOG_INFO("%s", "continue reading");
            continue;
        // 非阻塞IO，这个条件表示数据全部读取完毕
        // 非阻塞的系统调用，由于资源限制/不满足条件，导致返回值为EAGAIN
        } else if (bytes_read == -1 && ((errno == EAGAIN) || (errno == EWOULDBLOCK))) {
            break;
        // EOF，客户端断开连接
        } else if (bytes_read == 0) {  
            LOG_INFO("%s", "read EOF, client fd "+ std::to_string(sockFd) +" disconnected");
            stateConn = State::Closed;
            break;
        } else {
            LOG_INFO("%s", "Other error on client fd " + std::to_string(sockFd));
            stateConn = State::Closed;
            break;
        }
    }
}

void Connection::writeNonBlocking(){
    int sockFd = sock->getFd();
    int lenSendBuf  = sendBuf->size();
    // 字符串数组初始值太大会报错段错误
    const char* buf = sendBuf->cStr();
    int data_size = lenSendBuf;
    int data_left = lenSendBuf;

    while (data_left > 0) {
        ssize_t bytes_write = write(sockFd, buf + data_size - data_left, data_left);
        // 如果进程在一个慢系统调用(slow system call)中阻塞时，当捕获到某个信号且相应信号处理函数返回时，这个系统调用不再阻塞而是被中断，就会调用返回错误（一般为-1）&&设置errno为EINTR
        if (bytes_write == -1 && errno == EINTR) {
            LOG_INFO("%s", "continue writing");
            continue;
        }
        // 非阻塞的系统调用，由于资源限制/不满足条件，导致返回值为EAGAIN
        if (bytes_write == -1 && errno == EAGAIN) {
            sleep(0.5);
            continue;
        }
        if (bytes_write == -1) {
            LOG_INFO("%s", "Other error on client fd " + std::to_string(sockFd));
            stateConn = State::Closed;
            break;
        }
        data_left -= bytes_write;
    }
}

void Connection::readBlocking(){
    int sockFd = sock->getFd();
    char buf[2048];
    ssize_t bytes_read = read(sockFd, buf, sizeof(buf));
    if (bytes_read > 0) {
        readBuf->append(buf, bytes_read);
    } else if (bytes_read == 0) {
        LOG_INFO("%s", "read EOF, blocking client fd "+ std::to_string(sockFd) +" disconnected");
        stateConn = State::Closed;
    } else if (bytes_read == -1) {
        LOG_INFO("%s", "Other error on client fd " + std::to_string(sockFd));
        stateConn = State::Closed;
    }
}

void Connection::writeBlocking(){
    int sockFd = sock->getFd();
    // 没有处理send_buffer_数据大于TCP写缓冲区，的情况，可能会有bug
    ssize_t bytes_write = write(sockFd, sendBuf->cStr(), sendBuf->size());
    if (bytes_write == -1) {
        LOG_INFO("%s", "Other error on client fd " + std::to_string(sockFd));
        stateConn = State::Closed;
    }
}