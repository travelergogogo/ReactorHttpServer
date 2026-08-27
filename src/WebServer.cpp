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
                
                connections_[fd]->handleRead();
                
            }
        }
        // int clientfd=socket_.acceptClient();
        //
        //ep.addFd(clientfd,)

        // if(clientfd == -1)
        // {
        //     continue;
        // }

       
       
    }
}