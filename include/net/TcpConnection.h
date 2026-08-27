#pragma once
#include"net/Buffer.h"
class TcpConnection
{
public:

TcpConnection(int fd);

//void handle();

void handleRead();
private:

int fd_;

Buffer inputBuffer_;
};