#pragma once
#include<string>
class FileUtil
{

public:
//判断路径是否存在
    static bool exists(const std::string& path);
//读取文件
    static std::string readFile(const std::string&path);
private:

};