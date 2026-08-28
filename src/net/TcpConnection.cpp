#include"net/TcpConnection.h"
#include"http/HttpRequest.h"
#include"http/HttpResponse.h"
#include"net/Buffer.h"

#include<sys/socket.h>
#include<unistd.h>
#include<iostream>
#include<string>
#include<cerrno>

TcpConnection::TcpConnection(int fd):fd_(fd)
{

}



bool TcpConnection::handleRead()
{
    bool peerClosed = false;
    char buf[4096];

    while(true)
    {
       ssize_t n=recv(fd_,buf,sizeof(buf),0); 
       if(n>0)
       {
            inputBuffer_.append(buf,n);
            continue;
       }
       if(n==0)
        {
            peerClosed=true;
            break;
        }
        //暂时没数据了，可以先处理别的
        if(n<0)
        {
            if(errno == EAGAIN || errno == EWOULDBLOCK)
            {
                break;
            }
            if(errno == EINTR)
            {
         //表示信号被打断了，这时候重新接收
                continue;
            }
            perror("recv");
            return false;
            
        }
        
    }
    //客户端可能关闭，没有待处理数据，所以不能进行下面的操作，这种情况会返回false；EAGAIN + 没数据,return true;
    if(inputBuffer_.empty())
    {
        return !peerClosed;
    }
    std::string request = inputBuffer_.readAll();
    std::cout << "buffer size:" << request.size() << std::endl;

    std::cout << "----- request -----\n";
    std::cout << request << std::endl;


    HttpRequest req;
           
            
            
           
    if(!req.parse(request))
    {
        //close(fd_);
        return false;
    }   

    HttpResponse response;
    std::string data=response.makeResponse(req.path());

    send(fd_,data.c_str(),data.size(),0);
    return !peerClosed;

        
}

TcpConnection::~TcpConnection()
{
    close(fd_);
    std::cout << "connection closed fd="
          << fd_
          << std::endl;
}