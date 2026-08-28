#include"WebServer.h"
#include"http/HttpRequest.h"
#include"http/HttpResponse.h"
#include"net/TcpConnection.h"
#include"net/Epoll.h"

#include<sys/socket.h>
#include<sys/epoll.h>
#include<unistd.h>
#include<string>
#include<iostream>

WebServer::WebServer(int port):socket_(port)
{
    socket_.setNonBlocking();
    epoll_.addFd(socket_.fd(),EPOLLIN);
}

void WebServer::start()
{
    epoll_event events[1024];
    while(true)
    {
        int n =epoll_.wait(events,1024);
        
        for(int i=0;i<n;++i)
        {
            int fd =events[i].data.fd;


            if(fd == socket_.fd())
            {
                int clientfd =socket_.acceptClient();
                auto conn =std::make_shared<TcpConnection>(clientfd);

                connections_[clientfd]=conn;
                epoll_.addFd(clientfd,EPOLLIN);

                std::cout
                    << "new client fd="
                    << clientfd
                    << std::endl; 
                
            }
            else{
                //如果fd之前没有的话就会创建，然后再调用成员函数就出现UB了
                //connections_[fd]->handleRead();
                //所以先判断
                auto it=connections_.find(fd);
                if(it!=connections_.end())
                {
                    //判断连接是否还在
                    bool alive=it->second->handleRead();
                    if(!alive)
                    {
                        epoll_.delFd(fd);
                        connections_.erase(it);
                    }
                   
                }
                
                
            }
        }
        

       
       
    }
}