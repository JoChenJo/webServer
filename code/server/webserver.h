#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <unordered_map>
#include <fcntl.h>       // fcntl()
#include <unistd.h>      // close()
#include <assert.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "epoller.h"
#include "../log/log.h"
#include "../timer/heaptimer.h"
#include "../pool/sqlconnpool.h"
#include "../pool/threadpool.h"
#include "../pool/sqlconnRAII.h"
#include "../http/httpconn.h"

class WebServer {
public:
    WebServer(
        int port, int trigMode, int timeoutMS, bool OptLinger,      //端口 ET模式 timeoutMs(延迟时间) 优雅退出
        int sqlPort, const char* sqlUser, const  char* sqlPwd,      //sql端口   用户名  密码
        const char* dbName, int connPoolNum, int threadNum,         //数据库名  连接池数量  线程数量    
        bool openLog, int logLevel, int logQueSize);                //日志开关  日志等级    日志异步队列容量

    ~WebServer();
    void Start();                                                   //启动epoll_wait循环监听满足事件的fd，并处理各类事件

private:
    bool InitSocket_();                                             //初始化监听套接字
    void InitEventMode_(int trigMode);                              // 设置监听的文件描述符和通信的文件描述符的模式
    void AddClient_(int fd, sockaddr_in addr);                      //添加客户端信息到users_哈希表里
  
    void DealListen_();                                             //经营listen事件，等待通信套接字的连接accept
    void DealWrite_(HttpConn* client);                              //经营写事件，将写函数添加到线程池任务队列中
    void DealRead_(HttpConn* client);                               //经营读事件，将读函数添加到线程池任务队列中

    void SendError_(int fd, const char*info);                       //向客户端发送报错信息，发送失败日志报错，并关闭该fd
    void ExtentTime_(HttpConn* client);                             //调整客户端的超时时间节点
    void CloseConn_(HttpConn* client);                              //关闭客户端的连接

    void OnRead_(HttpConn* client);                                 //读事件的回调函数
    void OnWrite_(HttpConn* client);                                //写事件的回调函数
    void OnProcess(HttpConn* client);                               //修改客户端的事件

    static const int MAX_FD = 65536;    // 最大的文件描述符的个数

    static int SetFdNonblock(int fd);   // 设置文件描述符非阻塞

    int port_;          // 端口
    bool openLinger_;   // 是否打开优雅关闭
    int timeoutMS_;  /* 毫秒MS */
    bool isClose_;  // 是否关闭服务器
    int listenFd_;  // 监听的文件描述符
    char* srcDir_;  // 资源的目录
    
    uint32_t listenEvent_;  // 监听的文件描述符的事件
    uint32_t connEvent_;    // 连接的文件描述符的事件
   
    std::unique_ptr<HeapTimer> timer_;  // 定时器
    std::unique_ptr<ThreadPool> threadpool_;    // 线程池
    std::unique_ptr<Epoller> epoller_;      // epoll对象
    std::unordered_map<int, HttpConn> users_;   // 保存的是客户端连接的信息
};


#endif //WEBSERVER_H