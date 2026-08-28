#include"net/Epoll.h"

#include"sys/epoll.h"
#include<unistd.h>
#include<iostream>

Epoll::Epoll()
{
    epollfd_=epoll_create1(0);
    if(epollfd_==-1)
    {
        perror("epoll");
        exit(1);
    }
}

Epoll::~Epoll()
{
    close(epollfd_);
}
void Epoll::addFd(int fd,uint32_t events)
{
    epoll_event event{};
    event.events=events;
    event.data.fd=fd;
    int ret= epoll_ctl(epollfd_,EPOLL_CTL_ADD,fd,&event);
    if(ret==-1)
    {
        perror("eopll_ctl");
    }
}

int Epoll::wait(epoll_event *events,int maxEvents)
{
    return epoll_wait(epollfd_,events,maxEvents,-1);

  
}

void Epoll::delFd(int fd)
{
    int ret=epoll_ctl(epollfd_,EPOLL_CTL_DEL,fd,nullptr);
    if(ret==-1)
    {
        perror("epoll_ctl(del)");
    }
    //不能关闭，我们只是告诉epoll不关注这个fd了close(fd);
}