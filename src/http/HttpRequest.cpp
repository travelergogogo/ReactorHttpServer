#include"http/HttpRequest.h"
#include<sstream>

bool HttpRequest::parse(const std::string &request)
{
    std::istringstream iss(request);
    iss >>method_ >> path_>>version_;
    return !method_.empty()&&!path_.empty()&&!version_.empty();
}

const std::string& HttpRequest::method()const
{
    return this->method_;
}

const std::string& HttpRequest::path()const{
    return this->path_;
}

const std::string& HttpRequest::version()const{
    return this->version_;
}