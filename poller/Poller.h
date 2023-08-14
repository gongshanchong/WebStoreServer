#pragma once
#include <sys/epoll.h>
#include <vector>
#include <unistd.h>
#include <string.h>
#include "../utils/utils.h"
#include "../channel/Channel.h"

class Poller
{
private:
    int epfd;
    struct epoll_event *events_;
public:
    Poller();
    ~Poller();

    void updateChannel(Channel* ch);
    void deleteChannel(Channel* ch);

    void addEpoll(Channel* ch);
    void modEpoll(Channel* ch);
    void delEpoll(Channel* ch);

    std::vector<Channel*> poll(long timeout = -1) const;
};

