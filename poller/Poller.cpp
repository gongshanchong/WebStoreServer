#include "Poller.h"

#define MAX_EVENTS 1000

Poller::Poller() : epfd(-1), events_(nullptr){
    epfd = epoll_create1(0);
    errif(epfd == -1, "epoll create error");

    events_ = new epoll_event[MAX_EVENTS];
    bzero(events_, sizeof(*events_) * MAX_EVENTS);
}

Poller::~Poller(){
    if(epfd != -1){
        close(epfd);
        epfd = -1;
    }
    delete [] events_;
}

std::vector<Channel*> Poller::poll(long timeout) const{
    std::vector<Channel*> activeChannels;
    // timeout = -1表示调用将一直阻塞，直到有文件描述符进入ready状态或者捕获到信号才返回
    int nfds = epoll_wait(epfd, events_, MAX_EVENTS, timeout);
    // 如果进程在一个慢系统调用(slow system call)中阻塞时，当捕获到某个信号且相应信号处理函数返回时，这个系统调用不再阻塞而是被中断，就会调用返回错误（一般为-1）&&设置errno为EINTR
    errif(nfds == -1 && errno != EINTR, "epoll wait error");

    for(int i = 0; i < nfds; ++i){
        // 利用event结构体中的void指针指向channel来进行事件的处理
        Channel *ch = (Channel*)events_[i].data.ptr;
        // 获取epoll返回的事件并进行处理
        ch -> setReadyEvent(events_[i].events);
        activeChannels.push_back(ch);
    }

    return activeChannels;
}

void Poller::updateChannel(Channel *channel){
    if(!channel->getInEpoll()){
        this->addEpoll(channel);
        channel->setInEpoll();
    } else{
        this->modEpoll(channel);
    }
}

void Poller::deleteChannel(Channel *channel){
    this->delEpoll(channel);
    channel->setInEpoll(false);
}

void Poller::addEpoll(Channel *ch){
    int fd = ch->getFd();

    struct epoll_event ev;
    bzero(&ev, sizeof(ev));
    // 利用event结构体中的void指针指向channel来进行事件的处理
    ev.data.ptr = ch;
    ev.events = ch->getListenEvents();

    errif(epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) == -1, "epoll add error");
}

void Poller::modEpoll(Channel *ch){
    int fd = ch->getFd();

    struct epoll_event ev;
    bzero(&ev, sizeof(ev));
    // 利用event结构体中的void指针指向channel来进行事件的处理
    ev.data.ptr = ch;
    ev.events = ch->getListenEvents();

    errif(epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev) == -1, "epoll modify error");
}

void Poller::delEpoll(Channel *ch){
    int fd = ch->getFd();
    errif(epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL) == -1, "epoll delete error");
    close(fd);
}