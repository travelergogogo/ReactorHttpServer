#pragma once
#include<cstdint>
#include<sys/epoll.h>
class Epoll
{

public:
    Epoll();
    ~Epoll();
    void addFd(int fd,uint32_t events);

    int wait(epoll_event *events,int maxEvents);

    void delFd(int fd);

    
private:
    int epollfd_;
};