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
    char buf[4096];

    ssize_t n=recv(fd_,buf,sizeof(buf),0);
    
    std::cout << "recv size: "
          << n
          << std::endl;
    //对端关闭
    if(n==0)
    {
        return false;
    }
//不能这样，recv() 返回 0，表示客户端正常断开。但此时 errno 可能还残留着上一次系统调用的：EAGAIN，可能会误判

   // if(n <= 0)
   if(n<0)
    {
        if(errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return true;
        }

        if(errno == EINTR)
        {
        // 当前阶段先保留连接
        // 下一步改成 recv 循环时会直接 retry
            return true;
        }

        perror("recv");
        return false;
    }

   
            //按照 recv 返回的实际字节数 n 构造字符串，不依赖 C 字符串的 \0 结束符，因此适合处理网络数据。
            // std::string request(buf,n);

            inputBuffer_.append(buf,n);
            
            

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
            return true;

        
}

TcpConnection::~TcpConnection()
{
    close(fd_);
    std::cout << "connection closed fd="
          << fd_
          << std::endl;
}