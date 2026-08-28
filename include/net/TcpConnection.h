#pragma once
#include"net/Buffer.h"
class TcpConnection
{
public:

TcpConnection(int fd);

//void handle();

bool handleRead();

~TcpConnection();
private:

int fd_;

Buffer inputBuffer_;
};