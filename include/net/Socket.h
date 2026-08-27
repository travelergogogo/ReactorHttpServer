#pragma once
class Socket
{
public:
    explicit Socket(int port);//构造函数

    int fd()const;//获取文件描述符

    int acceptClient();

    void setNonBlocking();
private:
    int listenfd_;//监听文件描述符


};