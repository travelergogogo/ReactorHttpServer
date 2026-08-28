#include<iostream>

#include"net/Socket.h"

#include<sys/socket.h>
#include<netinet/in.h>
#include<unistd.h>
#include<fcntl.h>
//Socket构造函数类外实现
Socket::Socket(int port)
{
    listenfd_=socket(AF_INET,SOCK_STREAM,0);
    if(listenfd_==-1)
    {
        perror("socket");
    }
    //设置地址复用
    int opt=1;
    setsockopt(listenfd_,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));

    //设置地址族
    sockaddr_in addr{};
    addr.sin_family=AF_INET;
    addr.sin_port=htons(port);
    addr.sin_addr.s_addr=htonl(INADDR_ANY);
    //bind
    if(bind(listenfd_,reinterpret_cast<sockaddr*>(&addr),sizeof(addr))==-1)
    {
        perror("bind");
        close(listenfd_);
    }

    listen(listenfd_,128);

    setNonBlocking();

}

//accept接收函数
int Socket::acceptClient()
{
    
    //创建客户端地址结构，准备接收客户端IP和端口
    sockaddr_in clientAddr{};
    //长度
    socklen_t len=sizeof(clientAddr);
    int clientfd=accept(listenfd_,reinterpret_cast<sockaddr*>(&clientAddr),&len);
    if(clientfd<0)
    {

        return -1;
    }
    int flags=fcntl(clientfd,F_GETFL,0);
    if(flags==-1)
    {
        perror("fcntl");
        //一定要close不然会泄露，因为fcntl失败了不close就导致没人再知道这个文件标识符了
        close(clientfd);
        return -1;
    }
    flags|=O_NONBLOCK;
    if(fcntl(clientfd,F_SETFL,flags)==-1)
    {
        perror("fcntl(SET)");
        close(clientfd);
        return -1;
    }
    return clientfd;
}


int Socket::fd()const
{
    return listenfd_;
}

void Socket::setNonBlocking()
{
    int flags=fcntl(listenfd_,F_GETFL,0);

    flags|=O_NONBLOCK;

    fcntl(listenfd_,F_SETFL,flags);
}
