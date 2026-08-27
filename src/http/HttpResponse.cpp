#include"http/HttpResponse.h"
#include<string>
#include<iostream>
#include"utils/FileUtil.h"

std::string HttpResponse::makeResponse(const std::string& path)
{
    // std::cout<<"hello http"<<std::endl;
    std::string filePath="./www"+path;
    
    int statusCode = 200;

    std::string statusMessage = "OK";
    std::string body;
    if(FileUtil::exists(filePath))
    {
        body=FileUtil::readFile(filePath);
        
    }
    else{
        statusCode = 404;

        statusMessage = "Not Found";
        body = "<h1>404 Not Found</h1>";
    }

    std::string response;
    response += "HTTP/1.1 ";

    response += std::to_string(statusCode);

    response += " ";

    response += statusMessage;

    response += "\r\n";

    // 响应头
response += "Content-Type: text/html\r\n";

response += "Content-Length: ";

response += std::to_string(body.size());

response += "\r\n";


// 空行
response += "\r\n";


// 响应体
response += body;
    return response;
}

