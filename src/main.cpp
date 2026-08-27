#include<iostream>

#include"WebServer.h"
#include"net/Socket.h"

int main()
{
    WebServer server(8888);

    server.start();

    // Socket sever(8888);
    // std::cout<<"Socket created,"<<"listenfd="<<sever.fd()<<std::endl;

    // while(true)
    // {
    //     int clientfd=sever.acceptClient();
    //     std::cout<<"new client"<<clientfd<<std::endl;
    // }
    return 0;
}