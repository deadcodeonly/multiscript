#pragma once

#include <string>
#include <vector>

class Util
{
public:
    static std::string &ltrim(std::string &_s);
    static std::string &rtrim(std::string &_s);
    static std::string &trim(std::string &_s);
    static std::vector<std::string> split(const std::string &_str, const char &_separator, const bool &_checkEmpty = false);
    static std::string getHome();
private:
    Util();
    virtual ~Util();
    static const char* ws;
};