#pragma once

#include <string>

class LuaRun
{
public:
    static std::string historyFile;
    LuaRun();
    virtual ~LuaRun();
    int run(int argc, char *argv[]);
private:
    LuaRun(const LuaRun &_o) {}
    const LuaRun &operator=(const LuaRun &_o) {return *this;}
    struct Data;
    Data *d;
};
