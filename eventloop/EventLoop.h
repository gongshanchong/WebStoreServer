#pragma once
#include <functional>
#include <vector>
#include <memory>
#include "../channel/Channel.h"
#include "../poller/Poller.h"

class EventLoop
{
private:
    std::unique_ptr<Poller> poller;
    bool quit_;
public:
    EventLoop();
    ~EventLoop();

    void loop() const;
    void quit();
    void updateChannel(Channel* ch);
    void deleteChannel(Channel* ch);
};

