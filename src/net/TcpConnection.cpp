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

// void TcpConnection::handle()
// {
   
//     char buf[4096];

//     ssize_t n=recv(fd_,buf,sizeof(buf),0);
    
//     std::cout << "recv size: "
//           << n
//           << std::endl;

//     if(n <= 0)
//     {
//         close(fd_);
//         return;
//     }

   
//             //按照 recv 返回的实际字节数 n 构造字符串，不依赖 C 字符串的 \0 结束符，因此适合处理网络数据。
//             // std::string request(buf,n);

//             inputBuffer_.append(buf,n);
            
            

//             std::string request = inputBuffer_.readAll();
//             std::cout << "buffer size:" << request.size() << std::endl;

//             std::cout << "----- request -----\n";
//             std::cout << request << std::endl;


//             HttpRequest req;
           
            
            
           
//             if(!req.parse(request))
//             {
                
//                 return;
//             }   

//             HttpResponse response;
//             std::string data=response.makeResponse(req.path());

//             send(fd_,data.c_str(),data.size(),0);
        

//         close(fd_);
//         return;
// }

void TcpConnection::handleRead()
{
    char buf[4096];

    ssize_t n=recv(fd_,buf,sizeof(buf),0);
    
    std::cout << "recv size: "
          << n
          << std::endl;

    if(n <= 0)
    {
        if(errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return;
        }

        close(fd_);
        return;
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
                close(fd_);
                return;
            }   

            HttpResponse response;
            std::string data=response.makeResponse(req.path());

            send(fd_,data.c_str(),data.size(),0);
        

        
}