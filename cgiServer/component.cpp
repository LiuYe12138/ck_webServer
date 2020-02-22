#include"component.h"

int cgiConn :: epollFd ;

int tool :: setNoBlock(int fd) {
    int old = fcntl(fd, F_GETFL) ;
    int ret = fcntl(fd, F_SETFL, old|O_NONBLOCK) ;
    if(ret < 0) {
        std::cout << __FILE__ << "       " << __LINE__ << std::endl ;
        return -1 ;
    }
    return old ;
}

int tool::createEventFd() {
    int fd = eventfd(0, 0) ;
    if(fd < 0) {
        std::cout << __FILE__ << "      " << __LINE__ << std::endl ;
        return -1 ;
    }
    return fd ;
}

int tool ::createSocketPair(int* pipe) {
    if(pipe == NULL) return -1 ;
    int ret = socketpair(AF_UNIX, SOCK_STREAM, 0, pipe) ;
    if(ret < 0) {
        std::cout << __LINE__ << "      " << __FILE__ << std::endl ;
        return -1 ; 
    }
    return 1 ;
}


int tool :: addFd(int epollFd, int fd) {
    epoll_event ev ;
    ev.data.fd = fd ;
    ev.events = EPOLLIN|EPOLLET ;
    int ret = epoll_ctl(epollFd, EPOLL_CTL_ADD, fd, &ev) ;
    if(ret < 0) {
        std::cout << __FILE__ << "      " << __LINE__ << std::endl ;
        return -1 ;
    }
    ret = setNoBlock(fd) ;
    if(ret < 0) {
        std::cout << __LINE__ << "      " << __FILE__ <<std::endl ;
        return -1 ;
    }
    return 1 ;
}

int tool :: removeFd(int epollFd, int fd) {
   int ret = epoll_ctl(epollFd, EPOLL_CTL_DEL, fd, 0) ;
   if(ret < 0) {
       std::cout << __LINE__ << "     " << __FILE__ << std::endl ;
       return -1 ;
   }
   return 1 ;
}

void cgiConn::process() {
    while(true) {
        bzero(&cd, sizeof(cd)) ;
        int ret = recv(sockFd, &cd, sizeof(cd), 0) ;
        if(ret < 0) {
            if(errno != EAGAIN) {
                tool :: removeFd(epollFd, sockFd) ;
                break ;
            }
            return ;
        }
        else if(ret == 0) {
            int res = tool :: removeFd(epollFd, sockFd) ;
            if(res < 0) {
                return ;
            }
            break ;
        }
        else {
            if(access(cd.path, F_OK) == -1) {
                const char* error = "3\r\n404" ;
                if(send(sockFd, error, strlen(error), 0)< 0) {
                    std::cout << __FILE__ << "   " << __LINE__ << std::endl; 
                    break ;
                }
            }
            cgiProcess(sockFd, cd.path, cd.body) ;
            break ;
        }
    }
}
int cgiConn::cgiProcess(int sockFd, std::string path, std::string arg) {
    pid_t pid ;
    //复制管道描述复位标准输出
    dup2(sockFd, STDIN_FILENO) ;
    pid = fork() ;
    if(pid == 0) {
        execl(path.c_str(), arg.c_str()) ;
    }
    else {
        close(sockFd) ;
        int sta ;
        //等待子进程结束
        while((pid != waitpid(-1, &sta, WNOHANG))) ;
    }
    return 1 ;
}

void tool :: addSig(int sig, void(handle)(int), bool restart) {
    struct sigaction sa ;
    memset(&sa, '\0', sizeof(sa)) ;
    sa.sa_handler = handle ;
    if(restart) 
        int res = sa.sa_flags|SA_RESTART ;
    sigfillset(&sa.sa_mask) ;
}


int process :: createSocketPair() {
    tool::createSocketPair(pipe) ;
}   

