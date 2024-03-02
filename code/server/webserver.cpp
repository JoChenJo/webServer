#include "webserver.h"

using namespace std;

WebServer::WebServer(
            int port, int trigMode, int timeoutMS, bool OptLinger,                                  //端口 ET模式 timeoutMs(延迟时间) 优雅退出
            int sqlPort, const char* sqlUser, const  char* sqlPwd,                                  //sql端口   用户名  密码
            const char* dbName, int connPoolNum, int threadNum,                                     //数据库名  连接池数量  线程数量
            bool openLog, int logLevel, int logQueSize) :                                           //日志开关  日志等级    日志异步队列容量
            port_(port), openLinger_(OptLinger), timeoutMS_(timeoutMS),                             //初始化：端口 ET模式 timeoutMs(延迟时间)
            isClose_(false),timer_(new HeapTimer()),                                                //初始化：关闭状态    定时器
            threadpool_(new ThreadPool(threadNum)), epoller_(new Epoller())                         //初始化：线程池    epoll
    {
    srcDir_ = getcwd(nullptr, 256);                 // 获取当前的工作路径
    // /home/nowcoder/WebServer-master/resources/
    assert(srcDir_);
    strncat(srcDir_, "/resources/", 16);            //在当前工作路径后添加资源路径
    
    HttpConn::userCount = 0;                        //初始化已连接的客户端数量
    HttpConn::srcDir = srcDir_;                     //初始化客户端资源目录
    SqlConnPool::Instance()->Init("localhost", sqlPort, sqlUser, sqlPwd, dbName, connPoolNum);      //初始化数据库连接池

    // 初始化事件的模式
    InitEventMode_(trigMode);
   
    if(!InitSocket_()) { isClose_ = true;}                              //初始化监听套接字并将套接字挂在epoll上，初始化失败关闭客户端

    if(openLog) {                                                       //如果打开日志为true，则初始化日志
        Log::Instance()->init(logLevel, "./log", ".log", logQueSize);
        if(isClose_) { LOG_ERROR("========== Server init error!=========="); }
        else {
            LOG_INFO("========== Server init ==========");
            LOG_INFO("Port:%d, OpenLinger: %s", port_, OptLinger? "true":"false");
            LOG_INFO("Listen Mode: %s, OpenConn Mode: %s",
                            (listenEvent_ & EPOLLET ? "ET": "LT"),
                            (connEvent_ & EPOLLET ? "ET": "LT"));
            LOG_INFO("LogSys level: %d", logLevel);
            LOG_INFO("srcDir: %s", HttpConn::srcDir);
            LOG_INFO("SqlConnPool num: %d, ThreadPool num: %d", connPoolNum, threadNum);
        }
    }
}

WebServer::~WebServer() {                       //服务器析构函数
    close(listenFd_);                           //关闭套接字
    isClose_ = true;                            //关闭服务器
    free(srcDir_);                              //释放资源目录内存
    SqlConnPool::Instance()->ClosePool();       //关闭数据库连接池
}

// 设置监听的文件描述符和通信的文件描述符的模式
void WebServer::InitEventMode_(int trigMode) {
    listenEvent_ = EPOLLRDHUP;                  //设置监听套接字事件 EPOLLRDHUP:当对端关闭写入端时，会触发EPOLLRDHUP事件，表示读取的数据已经为空，此时可以断定对端已关闭连接。
    connEvent_ = EPOLLONESHOT | EPOLLRDHUP;     //设置通信套接字事件 EPOLLONESHOT:表示只监听一次事件,如果还需要继续监听这个socket的话，需要再次把这个socket加入到EPOLL队列里
    switch (trigMode)
    {
    case 0:                                     //0：啥也不干
        break;
    case 1:                                     //trigMode = 1
        connEvent_ |= EPOLLET;                  //通信套接字设置ET边沿触发模式
        break;
    case 2:                                     //trigMode = 2
        listenEvent_ |= EPOLLET;                //监听套接字设置ET边沿触发模式
        break;
    case 3:                                     //trigMode = 3
        listenEvent_ |= EPOLLET;                //监听套接字设置ET边沿触发模式
        connEvent_ |= EPOLLET;                  //通信套接字设置ET边沿触发模式
        break;
    default:
        listenEvent_ |= EPOLLET;                //监听套接字设置ET边沿触发模式
        connEvent_ |= EPOLLET;                  //通信套接字设置ET边沿触发模式
        break;
    }
    HttpConn::isET = (connEvent_ & EPOLLET);    //客户端事件设置为  EPOLLONESHOT | EPOLLRDHUP | EPOLLET
}

//启动epoll_wait循环监听满足事件的fd，并处理各类事件
void WebServer::Start() {                                               
    int timeMS = -1;  /* epoll wait timeout == -1 无事件将阻塞 */
    if(!isClose_) { LOG_INFO("========== Server start =========="); }
    while(!isClose_) {                                                  //服务器没关闭就循环
        
        if(timeoutMS_ > 0) {
            timeMS = timer_->GetNextTick();
        }

        int eventCnt = epoller_->Wait(timeMS);                          //epoll_wait函数，等待timeMS秒后触发,返回满足监听事件的fd数量

        for(int i = 0; i < eventCnt; i++) {
            /* 处理事件 */
            int fd = epoller_->GetEventFd(i);                           //获取满足事件的fd
            uint32_t events = epoller_->GetEvents(i);                   //获取满足事件的fd的事件
            
            if(fd == listenFd_) {                                       //如果是监听fd的话
                DealListen_();  // 处理监听的操作，接受客户端连接
            }

            else if(events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {      //如果是EPOLLRDHUP | EPOLLHUP | EPOLLERR事件的话
                assert(users_.count(fd) > 0);
                CloseConn_(&users_[fd]);                                // 关闭客户端连接，将该通信fd从epoll上摘下
            }

            else if(events & EPOLLIN) {                                 //如果是EPOLLIN事件的话
                assert(users_.count(fd) > 0);
                DealRead_(&users_[fd]);                                 // 处理读操作, 会将该客户端的读回调函数添加到线程池任务队列中
            }
            
            else if(events & EPOLLOUT) {                                //如果是EPOLLOUT事件的话
                assert(users_.count(fd) > 0);
                DealWrite_(&users_[fd]);                                // 处理写操作, 会将该客户端的写回调函数添加到线程池任务队列中
            } else {
                LOG_ERROR("Unexpected event");
            }
        }
    }
}

void WebServer::SendError_(int fd, const char*info) {                   //向该fd对应的客户端发送指定的报错消息，并关闭通信fd
    assert(fd > 0);
    int ret = send(fd, info, strlen(info), 0);
    if(ret < 0) {
        LOG_WARN("send error to client[%d] error!", fd);
    }
    close(fd);
}

void WebServer::CloseConn_(HttpConn* client) {                          //将通信fd从Epoll摘下，并关闭客户端的fd
    assert(client);
    LOG_INFO("Client[%d] quit!", client->GetFd());
    epoller_->DelFd(client->GetFd());
    client->Close();
}

void WebServer::AddClient_(int fd, sockaddr_in addr) {                  
    assert(fd > 0);
    users_[fd].init(fd, addr);                                          //将通信fd和对应客户端信息添加到users_表中
    if(timeoutMS_ > 0) {
        timer_->add(fd, timeoutMS_, std::bind(&WebServer::CloseConn_, this, &users_[fd]));  //将关闭客户端连接函数添加到定时器里
    }
    epoller_->AddFd(fd, EPOLLIN | connEvent_);                          //将通信fd添加到epoll上并设置读事件
    SetFdNonblock(fd);                                                  //设置通信fd为非阻塞
    LOG_INFO("Client[%d] in!", users_[fd].GetFd());
}

void WebServer::DealListen_() {                                         //经营accept监听事件
    struct sockaddr_in addr; // 保存连接的客户端的信息
    socklen_t len = sizeof(addr);
    do {
        int fd = accept(listenFd_, (struct sockaddr *)&addr, &len);     //返回有客户端连接返回通信fd
        if(fd <= 0) { return;}                                          //如果没有客户端连接，则跳出该函数
        else if(HttpConn::userCount >= MAX_FD) {
            SendError_(fd, "Server busy!");                             //如果客户端连接数上限了，则向客户端发送错误，并关闭通信fd
            LOG_WARN("Clients is full!");
            return;
        }
        AddClient_(fd, addr);   // 添加客户端
    } while(listenEvent_ & EPOLLET);                                    //如果监听fd设置边沿触发，则一直循环
}

void WebServer::DealRead_(HttpConn* client) {                           //经营读事件，给客户端设置定时器，并将读回调函数添加到任务队列中
    assert(client);
    ExtentTime_(client);
    threadpool_->AddTask(std::bind(&WebServer::OnRead_, this, client));
}

void WebServer::DealWrite_(HttpConn* client) {                          //经营写事件，给客户端设置定时器，并将写回调函数添加到任务队列中
    assert(client);
    ExtentTime_(client);
    threadpool_->AddTask(std::bind(&WebServer::OnWrite_, this, client));
}

void WebServer::ExtentTime_(HttpConn* client) {                         //给客户端设置定时器
    assert(client);
    if(timeoutMS_ > 0) { timer_->adjust(client->GetFd(), timeoutMS_); }
}

// 这个方法是在子线程中执行的
void WebServer::OnRead_(HttpConn* client) {                             //读回调函数，用来添加到任务队列中
    assert(client);
    int ret = -1;
    int readErrno = 0;
    ret = client->read(&readErrno); // 读取客户端的数据
    if(ret <= 0 && readErrno != EAGAIN) {
        CloseConn_(client);
        return;
    }
    // 处理，业务逻辑的处理
    OnProcess(client);
}

void WebServer::OnProcess(HttpConn* client) {                           //处理业务逻辑
    if(client->process()) {
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);        //如果解析完数据，将该通信fd设置写事件
    } else {
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLIN);         //如果没有解析数据，将该通信fd设置读事件
    }
}

void WebServer::OnWrite_(HttpConn* client) {                            //写回调函数，用来添加到任务队列中
    assert(client);
    int ret = -1;
    int writeErrno = 0;
    ret = client->write(&writeErrno);   //写数据给客户端
    if(client->ToWriteBytes() == 0) {
        /* 传输完成 */
        if(client->IsKeepAlive()) {     
            OnProcess(client);          
            return;
        }
    }
    else if(ret < 0) {
        if(writeErrno == EAGAIN) {  //如果没写完
            /* 继续传输 */
            epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);
            return;
        }
    }
    CloseConn_(client);
}

/* Create listenFd */
bool WebServer::InitSocket_() {                     
    int ret;
    struct sockaddr_in addr;
    if(port_ > 65535 || port_ < 1024) {
        LOG_ERROR("Port:%d error!",  port_);
        return false;
    }
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port_);
    struct linger optLinger = { 0 };            //控制SO_LINGER的选项
    if(openLinger_) {
        /* 优雅关闭: 直到所剩数据发送完毕或超时 */
        optLinger.l_onoff = 1;              //套接字尝试等待所有数据发送完毕
        optLinger.l_linger = 1;             //等待时间为1s，超时会关闭套接字，并将未发送数据丢弃
    }

    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);        //创建监听fd
    if(listenFd_ < 0) {
        LOG_ERROR("Create socket error!", port_);
        return false;
    }
                                                                                        //SOL_SOCKET设置套接字级别选项，SO_LINGER 选项决定了套接字是立即关闭还是等待所有数据发送完毕
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(optLinger));  //设置监听套接字在关闭时的行为，特别是关于如何处理未发送完的数据
    if(ret < 0) {
        close(listenFd_);
        LOG_ERROR("Init linger error!", port_);
        return false;
    }

    int optval = 1;
    /* 端口复用 */
    /* 只有最后一个套接字会正常接收数据。 */
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));   //SO_REUSEADDR，设置端口复用，optval=1为复用，0为不复用
    if(ret == -1) {
        LOG_ERROR("set socket setsockopt error !");
        close(listenFd_);
        return false;
    }

    ret = bind(listenFd_, (struct sockaddr *)&addr, sizeof(addr));  //绑定地址
    if(ret < 0) {
        LOG_ERROR("Bind Port:%d error!", port_);
        close(listenFd_);
        return false;
    }

    ret = listen(listenFd_, 6);                                     //设置监听上限
    if(ret < 0) {
        LOG_ERROR("Listen port:%d error!", port_);
        close(listenFd_);
        return false;
    }

    ret = epoller_->AddFd(listenFd_,  listenEvent_ | EPOLLIN);      //将监听fd添加到epoll上
    if(ret == 0) {
        LOG_ERROR("Add listen error!");
        close(listenFd_);
        return false;
    }
    SetFdNonblock(listenFd_);                                       //设置监听fd为非阻塞状态
    LOG_INFO("Server port:%d", port_);
    return true;
}

// 设置文件描述符非阻塞
int WebServer::SetFdNonblock(int fd) {
    assert(fd > 0);

    // int flag = fcntl(fd, F_GETFD, 0);
    // flag = flag  | O_NONBLOCK;
    // // flag  |= O_NONBLOCK;
    // fcntl(fd, F_SETFL, flag);


    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
}


