#include "Timer.h"

TimerNode::TimerNode(int _fd, std::string _sockaddr, int timeout){
    sockfd = _fd;
    sockaddr = _sockaddr;
    deleted = false;
    struct timeval now;
    gettimeofday(&now, NULL);
    // 以毫秒计
    expiredTime = (((now.tv_sec % 10000) * 1000) + (now.tv_usec / 1000)) + timeout;
}

TimerNode::~TimerNode(){}

void TimerNode::update(int timeout){
    struct timeval now;
    gettimeofday(&now, NULL);
    expiredTime = (((now.tv_sec % 10000) * 1000) + (now.tv_usec / 1000)) + timeout;
}

bool TimerNode::isValid(){
    struct timeval now;
    gettimeofday(&now, NULL);
    size_t temp = (((now.tv_sec % 10000) * 1000) + (now.tv_usec / 1000));

    if (temp < expiredTime)
        return true;
    else {
        this->setDeleted();
        return false;
    }
}

void TimerNode::setTimeoutCallBack(std::function<void(int, std::string)> const &callback){
    timeoutCallBack = std::move(callback);
}

TimerQueue::TimerQueue(){}

TimerQueue::~TimerQueue(){}

void TimerQueue::addTimer(std::shared_ptr<TimerNode> node){
    timerNodeQueue.push(node);
}

void TimerQueue::handleExpiredEvent(){
    while (!timerNodeQueue.empty()) {
        sptTimerNode ptimerNow = timerNodeQueue.top();

        if ((ptimerNow->getDeleted() == true) || (ptimerNow->isValid() == false)){
            ptimerNow->timeoutCallBack(ptimerNow->getFd(), ptimerNow->getAddr());
            timerNodeQueue.pop();
        }
        else
            break;
    }
    // 设置信号传送闹钟，即用来设置信号SIGALRM在经过参数seconds秒数后发送给目前的进程
    // 定时处理任务，重新定时以不断触发SIGALRM信号
    alarm(DEFAULT_EXPIRED_TIME / 1000);
}

// 因为静态成员属于整个类，而不属于某个对象，如果在类内初始化，会导致每个对象都包含该静态成员，这是矛盾的
// 能在类中初始化的静态成员只有一种，那就是静态常量成员
// 在类内声明，在类外初始化
int TimerManager::pipefd[2];

TimerManager::TimerManager(EventLoop *_loop){
    timerQueue = std::make_unique<TimerQueue>();

    // 创建一对套接字进行通信, 使用管道通信
    errif(socketpair(PF_UNIX, SOCK_STREAM, 0, TimerManager::pipefd) == -1, "pipe create error");

    // 设置定时器信号
    this->addSig(SIGALRM, TimerManager::sigHandle, false);
    
    // 通道信号发送接收端设置为非阻塞，信号接收端放入epoo里面用于监听信号事件
    // send是将信息发送给套接字缓冲区，如果缓冲区满了，则会阻塞，这时候会进一步增加信号处理函数的执行时间，为此，将其修改为非阻塞
    // 绑定到事件类
    setNonBlocking(TimerManager::pipefd[1]);
    setNonBlocking(TimerManager::pipefd[0]);
    channel = std::make_unique<Channel>(_loop, TimerManager::pipefd[0]);
    std::function<void()> cb = std::bind(&TimerManager::handleExpiredEvent, this);
    
    channel->setReadCallback(cb);
    channel->enableRead();
    channel->useET();
}

TimerManager::~TimerManager(){}

void TimerManager::addTimer(std::shared_ptr<TimerNode> node){
    timerQueue->addTimer(node);
}

void TimerManager::handleExpiredEvent(){
    timerQueue->handleExpiredEvent();
}

// 信号处理函数
// 信号处理函数中仅仅通过管道发送信号值，不处理信号对应的逻辑，缩短异步执行时间，减少对主程序的影响
void TimerManager::sigHandle(int sig){
    //为保证函数的可重入性，保留原来的errno
    int save_errno = errno;
    int msg = sig;
    send(TimerManager::pipefd[1], (char *)&msg, 1, 0);
    errno = save_errno;
}

// 设置信号函数
void TimerManager::addSig(int sig, void(handler)(int), bool restart){
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    // sa_handler是一个函数指针，指向信号处理函数
    sa.sa_handler = handler;
    if (restart)
        // SA_RESTART，使被信号打断的系统调用自动重新发起
        sa.sa_flags |= SA_RESTART;
    // sa_mask用来指定在信号处理函数执行期间需要被屏蔽的信号
    // 用来将参数set信号集初始化，然后把所有的信号加入到此信号集里
    sigfillset(&sa.sa_mask);
    // 对信号设置新的处理方式
    errif(sigaction(sig, &sa, NULL) == -1, "sighandle set error");
}