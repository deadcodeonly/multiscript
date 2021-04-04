#include <stdlib.h>
#include <stdio.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "LuaRun.h"

int main(int argc, char *argv[])
{
    LuaRun lua;
    return lua.run(argc, argv);
}

