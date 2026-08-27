#include"net/Buffer.h"


#include<string>

void Buffer::append(const char* data,size_t len )
{
    buffer_.append(data,len);

}

std::string Buffer::readAll()
{
    std::string data=buffer_;
  
    buffer_.clear();

    return data;
}

bool Buffer::empty()const
{
    return buffer_.empty();
}