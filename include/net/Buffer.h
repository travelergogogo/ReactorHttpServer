#pragma once
#include<string>


class Buffer
{
public:
    void append(const char* data,size_t len);

    std::string readAll();


    bool empty()const;
private:
    std::string buffer_;
};