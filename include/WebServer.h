#pragma once

#include"net/Socket.h"
#include"net/Epoll.h"


#include<unordered_map>
#include<memory>
class WebServer
{
public:
    WebServer(int port);

    void start();


private:
    Socket socket_;

    Epoll epoll_;
    
    std::unordered_map<int,std::shared_ptr<TcpConnection>>connections_;
};