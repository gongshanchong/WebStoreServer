#include "EventLoop.h"

EventLoop::EventLoop(){
    poller = std::make_unique<Poller>();
    quit_ = false;
}

EventLoop::~EventLoop(){}

void EventLoop::loop() const{
    while(!quit_){
        for (Channel *active_ch : poller->poll()) {
            active_ch->handleEvent();
        }
    }
}

void EventLoop::quit(){ quit_ = true;}

void EventLoop::updateChannel(Channel *ch){ poller->updateChannel(ch); }

void EventLoop::deleteChannel(Channel *ch){ poller->deleteChannel(ch); }
