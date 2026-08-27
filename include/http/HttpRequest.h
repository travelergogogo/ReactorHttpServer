#pragma once

#include<string>
class HttpRequest
{
public:
    //HttpRequest();
    bool parse(const std::string &request);

    const std::string&method()const;
    const std::string&path()const;
    const std::string&version()const;
private:
    std::string method_;
    std::string path_;
    std::string version_;
};