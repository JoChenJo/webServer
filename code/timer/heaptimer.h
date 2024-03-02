#ifndef HEAP_TIMER_H
#define HEAP_TIMER_H

#include <queue>
#include <unordered_map>
#include <time.h>
#include <algorithm>
#include <arpa/inet.h> 
#include <functional> 
#include <assert.h> 
#include <chrono>
#include "../log/log.h"

typedef std::function<void()> TimeoutCallBack;
typedef std::chrono::high_resolution_clock Clock;
typedef std::chrono::milliseconds MS;
typedef Clock::time_point TimeStamp;

struct TimerNode {
    int id;
    TimeStamp expires;
    TimeoutCallBack cb;
    bool operator<(const TimerNode& t) {
        return expires < t.expires;
    }
};
class HeapTimer {
public:
    HeapTimer() { heap_.reserve(64); }

    ~HeapTimer() { clear(); }
    
    void adjust(int id, int newExpires);

    void add(int id, int timeOut, const TimeoutCallBack& cb);

    void doWork(int id);

    void clear();

    void tick();

    void pop();

    int GetNextTick();

private:
    void del_(size_t i);                    //删除TimerNode数组中索引为i的元素
    
    void siftup_(size_t i);                 //将新增加节点往上调整

    bool siftdown_(size_t index, size_t n); //将新增加节点往下调整

    void SwapNode_(size_t i, size_t j);     //交换两个节点

    std::vector<TimerNode> heap_;           //TimerNode数组

    std::unordered_map<int, size_t> ref_;   //保存文件描述符和该文件描述符的索引
};

#endif //HEAP_TIMER_H