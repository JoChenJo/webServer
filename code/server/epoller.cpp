#include "epoller.h"

Epoller::Epoller(int maxEvent):epollFd_(epoll_create(512)), events_(maxEvent){
    assert(epollFd_ >= 0 && events_.size() > 0);
}

Epoller::~Epoller() {
    close(epollFd_);
}

bool Epoller::AddFd(int fd, uint32_t events) {                          //将fd以及该fd的事件添加到epollfd红黑树上
    if(fd < 0) return false;
    epoll_event ev = {0};
    ev.data.fd = fd;
    ev.events = events;
    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &ev);
}

bool Epoller::ModFd(int fd, uint32_t events) {                          //修改epoll里fd以及该fd的事件
    if(fd < 0) return false;
    epoll_event ev = {0};
    ev.data.fd = fd;
    ev.events = events;
    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &ev);
}

bool Epoller::DelFd(int fd) {                                           //删除ep里红黑树上的fd以及该fd的事件
    if(fd < 0) return false;
    epoll_event ev = {0};
    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, &ev);
}

int Epoller::Wait(int timeoutMs) {                                      //检测满足条件的fd
    return epoll_wait(epollFd_, &events_[0], static_cast<int>(events_.size()), timeoutMs);  //超过timeoutMs会返回一次epoll中满足条件的fd
}

int Epoller::GetEventFd(size_t i) const {                               //获取该事件的fd
    assert(i < events_.size() && i >= 0);
    return events_[i].data.fd;
}

uint32_t Epoller::GetEvents(size_t i) const {                           //获取事件
    assert(i < events_.size() && i >= 0);
    return events_[i].events;
}