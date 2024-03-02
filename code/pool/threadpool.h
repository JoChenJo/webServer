#pragma once
#include <mutex>
#include <condition_variable>
#include <thread>
#include <functional>
#include <queue>
#include <cassert>

class ThreadPool{
public:
    explicit ThreadPool(size_t threadCount = 8):pool_(std::make_shared<Pool>()){
            assert(threadCount > 0);                                    //assert宏通常用于在调试期间检查程序中的条件，如果条件不满足，则程序会在该点终止并显示一个错误消息
            for(size_t i = 0; i < threadCount; ++i){                    //创建threadCount个子线程

                std::thread([pool = pool_](){                           //std::thread(lambda)创建线程，形参是线程的工作函数
                    std::unique_lock<std::mutex> locker(pool->mtx);     //对pool->mtx进行加锁
                    while(true){
                        if(!pool->tasks.empty()){
                            auto task = std::move(pool->tasks.front()); //从任务队列中取出一个元素，使用move移动资源，避免拷贝资源浪费
                            pool->tasks.pop();                          //移除掉被取出的任务
                            locker.unlock();                            //解锁，将锁交给其他线程
                            task();                                     //线程执行自己的任务，因为task是在线程中定义的，所以每个线程都有自己独立的task，不会被其他线程影响
                            locker.lock();                              //重新上锁，进行下一次循环
                        }else if(pool->isClosed){
                            break;                                      //如果线程是关闭状态，退出循环，线程结束任务
                        }else {
                            pool->cond.wait(locker);                    //如果任务队列为空，线程就阻塞在这里
                        }
                    }
                }).detach();                                            //设置线程分离
            }
    }
    ThreadPool() = default;                                             //由编译器提供一个默认构造函数
    ThreadPool(ThreadPool&&) = default;                                 //由编译器提供默认移动构造函数
    
    ~ThreadPool(){                                                      
        if(static_cast<bool>(pool_)){                                   //如果pool_管理的内存不为空，则返回true
            {
                std::lock_guard<std::mutex> locker(pool_->mtx);         
                pool_->isClosed = true;                                 //将每个线程的关闭状态isClosed设置为true，满足第24行代码执行条件
            }
            pool_->cond.notify_all();                                   //唤醒所有的子线程，当任务队列为空时，所有子线程被回收
        }
    }

    template<typename F>
    void AddTask(F&& task){                                             //用模板代替类型，可以添加返回值为任意类型的函数
        {
            std::lock_guard<std::mutex> locker(pool_->mtx);     
            pool_->tasks.emplace(std::forward<F>(task));                //用forward保留形参task的类型和值，实现完美转发
        }   
        pool_->cond.notify_one();                                       //添加任务后唤醒一个线程去执行
    }

private:
    //结构体池子
    struct Pool
    {
        std::mutex mtx;                             //互斥锁
        std::condition_variable cond;               //条件变量
        bool isClosed = {false};                    //是否关闭线程池
        std::queue<std::function<void()>> tasks;    //任务队列
    };
    std::shared_ptr<Pool> pool_;                    //结构体池子对象
};