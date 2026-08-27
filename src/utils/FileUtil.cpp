#include"utils/FileUtil.h"
#include<string>
#include<filesystem>
#include<fstream>
#include<sstream>
bool FileUtil::exists(const std::string& path)
{
    return std::filesystem::exists(path);
}

std::string FileUtil::readFile(const std::string& path)
{
    //最后要返回的结构体
    std::string body;

    std::ifstream file(path);
    if(!file.is_open())
    {
        return "";
    }
    std::stringstream ss;
    ss<<file.rdbuf();

    return ss.str();
}