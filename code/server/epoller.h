#ifndef EPOLLER_H
#define EPOLLER_H

#include <sys/epoll.h> //epoll_ctl()
#include <fcntl.h>  // fcntl()
#include <unistd.h> // close()
#include <assert.h> // close()
#include <vector>
#include <errno.h>

class Epoller {
public:
    explicit Epoller(int maxEvent = 1024);

    ~Epoller();

    bool AddFd(int fd, uint32_t events);        //将fd以及该fd的事件添加到epollfd红黑树上

    bool ModFd(int fd, uint32_t events);        //修改epoll里fd以及该fd的事件

    bool DelFd(int fd);                         //删除ep里红黑树上的fd以及该fd的事件

    int Wait(int timeoutMs = -1);               //检测满足条件的fd

    int GetEventFd(size_t i) const;             //获取该事件的fd

    uint32_t GetEvents(size_t i) const;         //获取事件
        
private:
    int epollFd_;   // epoll_create()创建一个epoll对象，返回值就是epollFd

    std::vector<struct epoll_event> events_;     // 检测到的事件的集合 
};

#endif //EPOLLER_H