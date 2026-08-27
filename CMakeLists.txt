cmake_minimum_required(VERSION 3.20)

project(HttpServer)

set(CMAKE_CXX_STANDARD 20)

include_directories(include)

add_executable(
    HttpServer 
    src/main.cpp
    src/net/Socket.cpp
    src/WebServer.cpp
    src/http/HttpRequest.cpp
    src/http/HttpResponse.cpp
    src/utils/FileUtil.cpp
    src/net/TcpConnection.cpp
    src/net/Buffer.cpp
    src/net/Epoll.cpp)