#include "util.h"

#include <pwd.h>
#include <unistd.h>
#include <sys/types.h>

#include <sstream>
#include <iostream>

const char* Util::ws = " \t\n\r\f\v";

Util::Util()
{
}

Util::~Util()
{
}

std::string &Util::ltrim(std::string &_s)
{
    _s.erase(0, _s.find_first_not_of(ws));
    return _s;
}

std::string &Util::rtrim(std::string &_s)
{
    _s.erase(_s.find_last_not_of(ws) + 1);
    return _s;
}

std::string &Util::trim(std::string &_s)
{
    return rtrim(ltrim(_s));
}

std::vector<std::string> Util::split(const std::string &_str, const char &_separator, const bool &_checkEmpty)
{
    std::vector<std::string> ret;
    std::stringstream ss(_str);
    std::string l;
    while (std::getline(ss, l, _separator))
    {
        if (_checkEmpty && l.empty())
        {
            continue;
        }
        ret.push_back(l);
    }
    return ret;
}

std::string Util::getHome()
{
    std::string home;
    passwd *pw = getpwuid(getuid());
    if (pw != NULL && pw->pw_dir != NULL)
    {
        home = pw->pw_dir;
    }
    return home;
}