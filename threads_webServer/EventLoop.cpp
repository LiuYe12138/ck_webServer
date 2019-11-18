#include"EventLoop.h" 
#include "Call.h"

eventLoop :: eventLoop() {
    //开8线程
    //创建一个epoll
    threadNums = 8 ;
    many = 1 ;
    flag = 1 ;
    p = &flag  ;
    epPtr = std::make_shared<epOperation>() ;
    quit = false ;
    pool = make_shared<threadPool>(threadNums);
    for(int i=0; i<threadNums; i++) {
        queue<channel>ls ;
        queues.push_back(ls) ;
    }

    for(int i=0; i<threadNums; i++) {
        muteLst.push_back(shared_ptr<mutex>(new mutex)) ;
    }
}

eventLoop:: ~eventLoop() {
}

int eventLoop :: clearCloseChannel(std::vector<channel>&list_) {
    //从epoll中删除套接字
    std::map<int, channel>::iterator iter ;
    for(channel chl:list_) {
        int fds = std::move(chl.getFd()) ;
        epPtr->del(fds) ;
        close(fds) ;
        //从map列表中找到并删除
        iter = clList.find(fds) ;
        if(iter == clList.end()) {
            return -1 ;
        }
        clList.erase(iter) ;
    }
    return 1 ;
}

//创建双向管道，用来唤醒事件分离器
int loopInfo :: buildWakeFd() { 
    int ret = socketpair(AF_UNIX, SOCK_STREAM, 0, wakeupFd) ;
    if(ret < 0) {
        cout << __FILE__ << "           " <<__LINE__ << endl ;
        return ret ;
    }
    return 1 ;
}

//创建线程
void eventLoop :: runThread() {
    
    for(int i=0; i<threadNums; i++) {
        loopInfo infos ;
        infos.buildWakeFd() ;
        infos.setChannel() ;
        info.push_back(infos) ;
        infos.setId(i) ;
        auto func = bind(&eventLoop::round, this, placeholders::_1, placeholders::_2, placeholders::_3) ;
        if(infos.getChl() == nullptr) {
            return ;
        } 
        if(infos.getEp() == nullptr) {
            return ;
        }
        pool->commit(func, infos, infos.getChl(), infos.getEp()) ;
    }
}

//唤醒某个线程
int eventLoop :: wakeup(int fd) {
    static int  ret = 0  ;
    ret++ ;
    int res = send(fd, &ret, sizeof(ret), 0) ;
    if(res < 0) {
        cout << __FILE__ << "         " << __LINE__ <<"   " << strerror(errno)<< endl ;
        return -1 ;
    }
    return 1 ;
}

//为唤醒描述符设置channel
int loopInfo :: setChannel() {
       
    chl = make_shared<channel>() ;
    int fd = wakeupFd[1] ;
    //设置非阻塞
    setNoBlock(fd) ;
    chl->setFd(fd) ;
    chl->setEvents(READ) ;
    chl->setEp(ep) ;
   //为唤醒套接字设置epoll
    //现在不能添加到epoll中!!!
    ep->add(fd, READ) ;
    //设置读回调
    connection tmp ;
    tmp.setWakeChlCall(chl) ;
    return 1 ;
}

int loopInfo :: setNoBlock(int fd) {
    int ret = fcntl(fd, F_GETFL) ;
    ret |= O_NONBLOCK ;
    ret = fcntl(fd, F_SETFL, ret) ;
    return ret ;
}

void loopInfo :: wakeCb(channel* chl) {
    int fd = chl->getFd() ;
    int ret  = 0 ;
    int res= read(fd, &ret, sizeof(ret)) ;
    if(res < 0) {
        cout << __FILE__ << "         " << __LINE__ << endl ;   
        return  ;
    }
}


void loopInfo :: print() {
    map<int, shared_ptr<channel>>::iterator iter ;
    for(iter = chlList.begin(); iter != chlList.end(); iter++) {
        cout << iter->second->getFd() << endl ;
    }
}
//该loop中的事件数量
shared_ptr<channel> loopInfo :: search(int fd) { 
    if(chlList.find(fd) == chlList.end()) {
        return NULL ;
    }
    return chlList[fd] ;
}

//绑定匿名unix套接字
void eventLoop :: round(loopInfo loop, shared_ptr<channel>chl, shared_ptr<epOperation> ep) {
    int wakeFd = loop.getReadFd() ;
    vector<pair<int, channel>>ls ;
    int stop = 0 ;
    int num = loop.getId() ;
    //获取epoll
    if(chl == nullptr) {
        return ;
    }
    //为唤醒描述符添加id号码
    chl->setId(loop.getId()) ;
    //将当前唤醒的channel加入到list中
    chl->setWakeFd(wakeFd) ;
    loop.add(chl->getFd(), chl) ;
    vector<shared_ptr<channel>>actChl ;
    vector<shared_ptr<channel>> closeLst ;
    //将wakeFd加入到epoll中
    while(!stop) {
    
        int ret = ep->roundWait(loop, actChl) ;    
        if(ret < 0) {
            stop = true  ;
        }
        for(shared_ptr<channel> chl : actChl) {
            if(chl == nullptr) {
                break ;
            }
            //继续执行当前队列中的任务
            int fd = chl->getFd() ;
            if(fd == wakeFd) {
                int ii ;
                read(fd, &ii, sizeof(ii)) ;
                //先检查当前线程队列中是否有数据
                ls = doPendingFunc(num) ;
                //有数据就加入到队列中
                if(!ls.empty()) {
                    addQueue(ls, loop, ep) ;
                }/*
                int len = ls.size() ;
                for(int i=0; i<len; i++) {
                    int ret = ls[i].second.handleEvent(fd, loop.chlList) ;
                    if(ret == 0) {
                        closeLst.push_back(chl) ;
                    }
                }*/
                continue ;
            }
            //设置当前epoll句柄
            ret = chl->handleEvent(fd, loop.chlList) ;
            if(ret == 0) {
                closeLst.push_back(chl) ;
            }    
        }
        //清空关闭连接
        for(auto s : closeLst) {
            int fd = s->getFd() ;
            ep->del(fd) ;
            loop.clearChannel(fd) ;
            close(fd) ;
        }
        closeLst.clear() ;
        //处理完成，清空队
        actChl.clear() ;
    }
}

int eventLoop :: readQueue(vector<pair<int, channel>>&ls, loopInfo& loop) {
    for(auto s : ls) {
        s.second.handleEvent(s.first, loop.chlList) ;
    }
    return 0 ;
}
void loopInfo::clearChannel(int fd) {
    auto ret = chlList.find(fd) ;
    if(ret != chlList.end()) {
        chlList.erase(ret) ;
        ep->del(fd) ;
        close(fd) ;
    }   
}

//唤醒线程
vector<pair<int, channel>> eventLoop :: doPendingFunc(int& num) {
    vector<pair<int, channel>> tmp ;
    if(queues[num].empty()) {
        return tmp ;
    }
    else {
        lock_guard<mutex> lk(*muteLst[num]) ;
        //计算当前队列中的要取的事件数量
       // int size = queues[num].size() ;
        /*while(!queues[num].empty()&&size--) {
            channel chl = queues[num].front() ;
            tmp.push_back({chl.getFd(), chl}) ;
            queues[num].pop() ;
        }*/
        
        while(!queues[num].empty()) {
            channel chl = queues[num].front() ;
            tmp.push_back({chl.getFd(), chl}) ;
            queues[num].pop() ;
        }
    }
    return tmp ;
}   


void eventLoop :: addQueue(vector<pair<int, channel>>&ls, 
                           loopInfo&loop, 
                           shared_ptr<epOperation>ep) {
    //将新连接加入到epoll中,操作局部变量线程安全
    for(pair<int, channel>p:ls) {
        //设置可读事件并加入到epoll中
        p.second.setEp(ep) ;
        
        ep->add(p.first, READ) ;
        p.second.setWakeFd(loop.getReadFd()) ;
        //将fd加入到事件表中
        loop.add(p.first, shared_ptr<channel>(new channel(p.second))) ;
    }
}

int loopInfo :: delChl(int fd, map<int, shared_ptr<channel>>& tmp) {
    auto iter = tmp.find(fd) ;
    if(iter == tmp.end()) {
        return -1 ;
    }
    //删除fd对应的事件
    tmp.erase(iter) ;
    return 1 ;
} 

int eventLoop:: getNum() {
    static int num = -1 ;
    num++ ;
    if(num == threadNums) {
        num = 0 ;
    }
    return num ;
}

void eventLoop :: loop() {
    
    runThread() ;
    //将conn加入到epoll中 
    int events = clList[servFd].getEvents() ;
    //将当前的服务加入到epoll中
    epPtr->add(servFd, events) ;
    map<int, shared_ptr<channel>> cpList ;
    vector<channel> closeList ;
    while(!quit) {
        copyClList(cpList) ;
        //等待事件
        int ret = epPtr->wait(this, many) ;
        if(ret < 0) {
            quit = true ;
        }
        //epoll返回０，没有活跃事件
        else if(ret == 0) {
        }
        //将事件分发给各个线程
        else {
            //处理连接，所有连接事件分给各个线程中的reactor
            for(channel chl : activeChannels) {
                if(many == 0) {
                    chl.handleEvent(chl.getFd(), cpList) ;
                    continue ;
                }
                else {
                    //要是不是多线程的话，就将事件传给子线程处理
                    int num = getNum() ; 
                    queueInLoop(chl, num) ;
                }
            }
            activeChannels.clear() ;
            if(many != 0 && closeList.empty()) continue ;
            for(auto s : closeList) {
                auto ret = cpList.find(s.getFd());
                if(ret != cpList.end()) {
                    cpList.erase(ret) ;  
                }   
            }
            //清除掉关闭连接
            //重新讲cpList中的数据转移到clList中
            for(auto res : cpList) {
                clList[res.first] = *(res.second) ;
            }
            //重置clList
            clList.clear() ;
            closeList.clear() ;
            //清空活跃事件集合
            activeChannels.clear() ;
        }
    }   
}

void eventLoop :: copyClList(map<int, shared_ptr<channel>>&ls) {
    for(auto s: clList) {
        ls.insert({s.first, shared_ptr<channel>(new channel(s.second))}) ;
    }
}
//往队列中加数据
int eventLoop :: queueInLoop(channel chl, int& num) {
    //向队列中插入元素
    //通知线程接收新连接
    int fd = info[num].getWriteFd() ;
    chl.setWakeFd(info[num].getReadFd()) ;
    {
        lock_guard<mutex> lk(*muteLst[num]) ;
        queues[num].push(chl) ;
    }
    wakeup(fd) ;
    return 1 ;
}   

//将活跃的事件加入到活跃事件表中
int eventLoop :: fillChannelList(channel chl) {
   activeChannels.push_back(chl) ;
   return 1 ;
}

//值有监听套接字拥有connection
//向loop中添加新的conn.将这个连接加入到epoll中
void eventLoop :: addConnection(connection* con) {
       
    conn = con ;
    std::shared_ptr<channel> chl = con->getChannel() ;  
    int fd = con->getSock()->getListenSock() ;
    servFd = fd ;
    //将这个服务器监听套接字加入到epoll中，只负责监听可读事件，LT模式
    chl->setFd(fd) ;
    chl->setEp(epPtr) ;
   // epPtr->add(fd, READ) ;
    chl->setEpFd(epPtr->getEpFd()) ;
    chl->setEvents(READ) ;
    //将fd和channel作为键值存入channelMap中
    clList[fd] = *chl ;
    //将这个服务器监听套接字加入到epoll中，只负责监听可读事件，LT模式
}

void eventLoop :: addClList(int fd, channel& channel_) {
    
    clList.insert({fd, channel_}) ;
    epPtr->add(fd, READ) ;
}   

channel* eventLoop :: search(int fd) {
    
    std::map<int, channel> :: iterator it = clList.find(fd) ;
    if(it == clList.end()) {
        return NULL ;
    }
    else {
        return &(it->second) ;
    }
} 

//接收连接并添加channel
channel eventLoop :: handleAccept() {
    channel tmp;
    tmp.setSock(conn->getSock()) ;
    //创建新连接
    int conFd = tmp.handleAccept(servFd) ;
    tmp.setFd(conFd) ;
    //为channel设置回调
    //设置套接字非阻塞
    conn->setnoBlocking(conFd) ;
    tmp.setEvents(READ) ;
    //给channel设置回调函数
    conn->setCallBackToChannel(&tmp) ; 
    //将channel加入到当前loop的列表中
    return tmp ;
}

