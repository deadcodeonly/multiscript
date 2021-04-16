#include "LuaRun.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>

#include <iostream>
#include <sstream>

#include <lua.hpp>
#include <readline/readline.h>
#include <readline/history.h>

#include "util.h"
#include "sshcontext.h"

#if !defined(LUA_PROGNAME)
#define LUA_PROGNAME		"lua"
#endif

#if !defined(LUA_INIT_VAR)
#define LUA_INIT_VAR		"LUA_INIT"
#endif

#define LUA_INITVARVERSION	LUA_INIT_VAR LUA_VERSUFFIX

#define has_error	1	/* bad option */
#define has_i		2	/* -i */
#define has_v		4	/* -v */
#define has_e		8	/* -e */
#define has_E		16	/* -E */

#define lua_initreadline(L)  ((void)L)
#define lua_readline(L,b,p) \
        ((void)L, fputs(p, stdout), fflush(stdout), \
        fgets(b, LUA_MAXINPUT, stdin) != NULL)
#define lua_saveline(L,line)	{ (void)L; (void)line; }
#define lua_freeline(L,b)	{ (void)L; (void)b; }
#define EOFMARK		"<eof>"
#define marklen		(sizeof(EOFMARK)/sizeof(char) - 1)
#define LUA_PROMPT		"> "
#define LUA_PROMPT2		">> "
#define LUA_MAXINPUT		512
#define lua_stdin_is_tty()	isatty(0)

std::string LuaRun::historyFile = "";

extern "C"
{
    static int protected_main(lua_State *L);
    static lua_State *globalL;
    static const char *progname;
    static int sshsetconfigpath_l(lua_State *L);
    static int sshgetconfigpath_l(lua_State *L);
    static int sshsetservergroup_l(lua_State *L);
    static int sshgetservergroup_l(lua_State *L);
    static int sshlsgroups_l(lua_State *L);
    static int sshaddserver_l(lua_State *L);
    static int sshrmserver_l(lua_State *L);
    static int sshaddgroup_l(lua_State *L);
    static int sshrmgroup_l(lua_State *L);
    static int sshlsserver_l(lua_State *L);
    static int sshlsservers_l(lua_State *L);
    static int sshgroup_l(lua_State *L);
    static int sshgetgroup_l(lua_State *L);
    static int sshcd_l(lua_State *L);
    static int sshpwd_l(lua_State *L);
    static int sshreadconfig_l(lua_State *L);
    static int sshexec_l(lua_State *L);
    static int sshexecs_l(lua_State *L);
    static int sshnohup_l(lua_State *L);
    static int sshnohups_l(lua_State *L);
    static int sshhelp_l(lua_State *L);
    static const struct luaL_Reg sshlib [] =
    {
        {"sshsetconfigpath", sshsetconfigpath_l},
        {"sshgetconfigpath", sshgetconfigpath_l},
        {"sshsetservergroup", sshsetservergroup_l},
        {"sshgetservergroup", sshgetservergroup_l},
        {"sshlsgroups", sshlsgroups_l},
        {"sshaddserver", sshaddserver_l},
        {"sshrmserver", sshrmserver_l},
        {"sshaddgroup", sshaddgroup_l},
        {"sshrmgroup", sshrmgroup_l},
        {"sshlsserver", sshlsserver_l},
        {"sshlsservers", sshlsservers_l},
        {"sshgroup", sshgroup_l},
        {"sshgetgroup", sshgetgroup_l},
        {"sshcd", sshcd_l},
        {"sshpwd", sshpwd_l},
        {"sshreadconfig", sshreadconfig_l},
        {"sshexec", sshexec_l},
        {"sshexecs", sshexecs_l},
        {"sshnohup", sshnohup_l},
        {"sshnohups", sshnohups_l},
        {"sshhelp", sshhelp_l},
        {NULL, NULL}
    };
}

//========== Exported Functions ==========//

static int sshsetconfigpath_l(lua_State *L)
{
    SSHContext &ctx = SSHContext::Factory();
    const char *path = luaL_checkstring(L, 1);
    ctx.setOtherConfigPath(path);
    return 0;
}

static int sshgetconfigpath_l(lua_State *L)
{
    SSHContext &ctx = SSHContext::Factory();
    lua_pushstring(L, ctx.getOtherConfigPath().c_str());
    return 1;
}

static int sshsetservergroup_l(lua_State *L)
{
    std::map<std::string, std::set<std::string> > newMap;
    lua_settop(L, 1);
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_pushnil(L);
    while(lua_next(L, 1) != 0)
    {
        luaL_checktype(L, -2, LUA_TSTRING);
        std::string k = lua_tostring(L, -2);
        luaL_checktype(L, -1, LUA_TTABLE);
        lua_pushnil(L);
        std::set<std::string> newSet;
        while(lua_next(L, -2) != 0)
        {
            luaL_checktype(L, -1, LUA_TSTRING);
            std::string v = lua_tostring(L, -1);
            newSet.insert(v);
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
        newMap[k] = newSet;
    }
    lua_settop(L, 0);
    SSHContext &ctx = SSHContext::Factory();
    ctx.setServerGroup(newMap);
    return 0;
}

static int sshgetservergroup_l(lua_State *L)
{
    lua_settop(L, 0);
    lua_newtable(L);
    SSHContext &ctx = SSHContext::Factory();
    std::map< std::string, std::set<std::string> > serverGroup = ctx.getServerGoup();
    for(std::map< std::string, std::set<std::string> >::iterator it1 = serverGroup.begin(); it1 != serverGroup.end(); ++it1)
    {
        const std::string &group = it1->first;
        const std::set<std::string> &servers = it1->second;
        lua_newtable(L);
        int i = 0;
        for(std::set<std::string>::iterator it2 = servers.begin(); it2 != servers.end(); ++it2)
        {
            ++i;
            const std::string &s = *it2;
            lua_pushstring(L, s.c_str());
            lua_rawseti(L, -2, i);
        }
        lua_setfield(L, -2, group.c_str());
    }
    return 1;
}

static int sshlsgroups_l(lua_State *L)
{
    lua_settop(L, 0);
    std::stringstream ss;
    SSHContext &ctx = SSHContext::Factory();
    std::map< std::string, std::set<std::string> > serverGroup = ctx.getServerGoup();
    for(std::map< std::string, std::set<std::string> >::iterator it1 = serverGroup.begin(); it1 != serverGroup.end(); ++it1)
    {
        const std::string &group = it1->first;
        const std::set<std::string> &servers = it1->second;
        ss << "[" << group << "]" << std::endl;
        for(std::set<std::string>::iterator it2 = servers.begin(); it2 != servers.end(); ++it2)
        {
            const std::string &s = *it2;
            ss << "    " << s << std::endl;
        }
    }
    lua_pushstring(L, ss.str().c_str());
    return 1;
}

static int sshaddserver_l(lua_State *L)
{
    lua_settop(L, 2);
    const char *group = luaL_checkstring(L, 1);
    const char *server = luaL_checkstring(L, 2);
    SSHContext &ctx = SSHContext::Factory();
    ctx.addServer(group, server);
    lua_settop(L, 0);
    return 0;
}

static int sshrmserver_l(lua_State *L)
{
    lua_settop(L, 2);
    const char *group = luaL_checkstring(L, 1);
    const char *server = luaL_checkstring(L, 2);
    SSHContext &ctx = SSHContext::Factory();
    ctx.removeServer(group, server);
    lua_settop(L, 0);
    return 0;
}

static int sshaddgroup_l(lua_State *L)
{
    lua_settop(L, 2);
    const char *group = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    lua_pushnil(L);
    std::set<std::string> servers;
    while(lua_next(L, 2) != 0)
    {
        const char *server = luaL_checkstring(L, 4);
        servers.insert(server);
        lua_pop(L, 1);
    }
    SSHContext &ctx = SSHContext::Factory();
    ctx.addGroup(group, servers);
    lua_settop(L, 0);
    return 0;
}

static int sshrmgroup_l(lua_State *L)
{
    lua_settop(L, 1);
    const char *group = luaL_checkstring(L, 1);
    SSHContext &ctx = SSHContext::Factory();
    ctx.removeGroup(group);
    lua_settop(L, 0);
    return 0;
}

static int sshlsserver_l(lua_State *L)
{
    lua_settop(L, 1);
    const char *group = luaL_checkstring(L, 1);
    SSHContext &ctx = SSHContext::Factory();
    std::set<std::string> servers = ctx.listGroup(group);
    lua_settop(L, 0);
    lua_newtable(L);
    int i = 0;
    for(std::set<std::string>::iterator it = servers.begin(); it != servers.end(); ++it)
    {
        ++i;
        lua_pushstring(L, it->c_str());
        lua_rawseti(L, 1, i);
    }
    return 1;
}

static int sshlsservers_l(lua_State *L)
{
    lua_settop(L, 1);
    const char *group = luaL_checkstring(L, 1);
    SSHContext &ctx = SSHContext::Factory();
    std::set<std::string> servers = ctx.listGroup(group);
    lua_settop(L, 0);
    std::stringstream ss;
    for(std::set<std::string>::iterator it = servers.begin(); it != servers.end(); ++it)
    {
        ss << it->c_str() << std::endl;
    }
    lua_pushstring(L, ss.str().c_str());
    return 1;
}

static int sshgroup_l(lua_State *L)
{
    lua_settop(L, 1);
    const char *group = luaL_checkstring(L, 1);
    SSHContext &ctx = SSHContext::Factory();
    ctx.setDefaultGroup(group);
    lua_settop(L, 0);
    return 0;
}

static int sshgetgroup_l(lua_State *L)
{
    lua_settop(L, 0);
    SSHContext &ctx = SSHContext::Factory();
    std::string s = ctx.getDefalultGroup();
    lua_pushstring(L, s.c_str());
    return 1;
}

static int sshcd_l(lua_State *L)
{
    lua_settop(L, 1);
    const char *path = luaL_checkstring(L, 1);
    SSHContext &ctx = SSHContext::Factory();
    ctx.setDefaultPath(path);
    lua_settop(L, 0);
    return 0;
}

static int sshpwd_l(lua_State *L)
{
    lua_settop(L, 0);
    SSHContext &ctx = SSHContext::Factory();
    std::string s = ctx.getDefaultPath();
    lua_pushstring(L, s.c_str());
    return 1;
}

static int sshreadconfig_l(lua_State *L)
{
    lua_settop(L, 1);
    const char *path = luaL_checkstring(L, 1);
    SSHContext &ctx = SSHContext::Factory();
    bool ret = ctx.readConfig(path);
    lua_settop(L, 0);
    if (ret)
    {
        lua_pushboolean(L, 1);
    }
    else
    {
        lua_pushboolean(L, 0);
    }
    return 1;
}

static int sshexec_l(lua_State *L)
{
    lua_settop(L, 1);
    const char *cmd = luaL_checkstring(L, 1);
    lua_settop(L, 0);
    try
    {
        SSHContext &ctx = SSHContext::Factory();
        ctx.setNohup(false);
        std::pair< bool, std::map< std::string, SSHContext::CommandOut > > ret = ctx.execCommand(cmd);
        if (ret.first)
        {
            lua_pushboolean(L, 1);
        }
        else
        {
            lua_pushboolean(L, 0);
        }
        int i;
        lua_newtable(L);
        for (std::map<std::string, SSHContext::CommandOut>::iterator it = ret.second.begin(); it != ret.second.end(); ++it)
        {
            lua_newtable(L);
            lua_pushinteger(L, it->second.status);
            lua_setfield(L, -2, "status");
            lua_newtable(L);
            i = 0;
            for (std::list<std::string>::iterator it2 = it->second.stdout.begin(); it2 != it->second.stdout.end(); ++it2)
            {
                ++i;
                lua_pushstring(L, it2->c_str());
                lua_rawseti(L, -2, i);
            }
            lua_setfield(L, -2, "stdout");
            lua_newtable(L);
            i = 0;
            for (std::list<std::string>::iterator it2 = it->second.stderr.begin(); it2 != it->second.stderr.end(); ++it2)
            {
                ++i;
                lua_pushstring(L, it2->c_str());
                lua_rawseti(L, -2, i);
            }
            lua_setfield(L, -2, "stderr");
            lua_setfield(L, -2, it->first.c_str());
        }
    }
    catch(const std::exception& e)
    {
        return luaL_error(L, e.what());
    }
    return 2;
}

int sshexecs_l(lua_State *L)
{
    lua_settop(L, 1);
    const char *cmd = luaL_checkstring(L, 1);
    lua_settop(L, 0);
    std::stringstream ss;
    try
    {
        SSHContext &ctx = SSHContext::Factory();
        ctx.setNohup(false);
        std::pair< bool, std::map< std::string, SSHContext::CommandOut > > ret = ctx.execCommand(cmd);
        std::string s;
        if (ret.first)
        {
            std::stringstream s2;
            s2 << std::endl << "All executions: OK" << std::endl;
            s = s2.str();
        }
        else
        {
            std::stringstream s2;
            s2 << std::endl << "Any executions: ERROR" << std::endl;
            s = s2.str();
        }
        for (std::map<std::string, SSHContext::CommandOut>::iterator it = ret.second.begin(); it != ret.second.end(); ++it)
        {
            ss << std::endl << "Server: " << it->first << " Status: " << it->second.status << std::endl;
            ss << "\t stdout:" << std::endl;
            for (std::list<std::string>::iterator it2 = it->second.stdout.begin(); it2 != it->second.stdout.end(); ++it2)
            {
                ss << "\t\t" << *it2 << std::endl;
            }
            ss << "\t stderr:" << std::endl;
            for (std::list<std::string>::iterator it2 = it->second.stderr.begin(); it2 != it->second.stderr.end(); ++it2)
            {
                ss << "\t\t" << *it2 << std::endl;
            }
        }
        ss << s << std::endl;
    }
    catch(const std::exception& e)
    {
        ss << e.what() << std::endl;
    }
    lua_pushstring(L, ss.str().c_str());
    return 1;
}

static int sshnohup_l(lua_State *L)
{
    lua_settop(L, 1);
    const char *cmd = luaL_checkstring(L, 1);
    lua_settop(L, 0);
    try
    {
        SSHContext &ctx = SSHContext::Factory();
        ctx.setNohup(true);
        std::pair< bool, std::map< std::string, SSHContext::CommandOut > > ret = ctx.execCommand(cmd);
        if (ret.first)
        {
            lua_pushboolean(L, 1);
        }
        else
        {
            lua_pushboolean(L, 0);
        }
        int i;
        lua_newtable(L);
        for (std::map<std::string, SSHContext::CommandOut>::iterator it = ret.second.begin(); it != ret.second.end(); ++it)
        {
            lua_newtable(L);
            lua_pushinteger(L, it->second.status);
            lua_setfield(L, -2, "status");
            lua_newtable(L);
            i = 0;
            for (std::list<std::string>::iterator it2 = it->second.stderr.begin(); it2 != it->second.stderr.end(); ++it2)
            {
                ++i;
                lua_pushstring(L, it2->c_str());
                lua_rawseti(L, -2, i);
            }
            lua_setfield(L, -2, "stderr");
            lua_setfield(L, -2, it->first.c_str());
        }
    }
    catch(const std::exception& e)
    {
        return luaL_error(L, e.what());
    }
    return 2;
}

static int sshnohups_l(lua_State *L)
{
    lua_settop(L, 1);
    const char *cmd = luaL_checkstring(L, 1);
    lua_settop(L, 0);
    std::stringstream ss;
    try
    {
        SSHContext &ctx = SSHContext::Factory();
        ctx.setNohup(true);
        std::pair< bool, std::map< std::string, SSHContext::CommandOut > > ret = ctx.execCommand(cmd);
        std::string s;
        if (ret.first)
        {
            std::stringstream s2;
            s2 << std::endl << "All executions: OK" << std::endl;
            s = s2.str();
        }
        else
        {
            std::stringstream s2;
            s2 << std::endl << "Any executions: ERROR" << std::endl;
            s = s2.str();
        }
        for (std::map<std::string, SSHContext::CommandOut>::iterator it = ret.second.begin(); it != ret.second.end(); ++it)
        {
            ss << std::endl << "Server: " << it->first << " Status: " << it->second.status << std::endl;
            if (! it->second.stderr.empty())
            {
                ss << "\t error: ";
                for (std::list<std::string>::iterator it2 = it->second.stderr.begin(); it2 != it->second.stderr.end(); ++it2)
                {
                    ss << *it2 << std::endl;
                }
            }
        }
        ss << s << std::endl;
    }
    catch(const std::exception& e)
    {
        ss << e.what() << std::endl;
    }
    lua_pushstring(L, ss.str().c_str());
    return 1;
}

static int sshhelp_l(lua_State *L)
{
    lua_settop(L, 0);
    std::string s = ""
        "nil sshsetconfigpath(string)      : Set new path to config file (multiscript.ini like) to load server groups.\n"
        "string sshgetconfigpath()         : Get path to config file.\n"
        "nil sshsetservergroup(luatable)   : Programatic form to 'multiscript.ini' ex.:\n"
        "                                        sshsetservergroup({nodes={'server1','server2'},gpu={'server3'}})\n"
        "luatable sshgetservergroup()      : Return a luatable with server groups.\n"
        "string sshlsgroups()              : Formated string with server groups.\n"
        "nil sshaddserver(string,string)   : Add server (string 2) to group (string 1).\n"
        "nil sshrmserver(string,string)    : Removes a server (string 2) from a group (string 1).\n"
        "nil sshaddgroup(string, luatable) : Add a new group ex.: sshaddgroup('nodes',{'server1','server2'}).\n"
        "nil sshrmgroup(stirng)            : Removes a server group.\n"
        "luatable sshlsserver()            : Return a luatable with all server groups.\n"
        "string sshlsservers()             : Return a formated string with all server groups.\n"
        "nil sshgroup(string)              : Set a server group to receive remote commands.\n"
        "string sshgetgroup()              : Return atual group name.\n"
        "nill sshcd(string)                : Set work directory to remote commands.\n"
        "string sshpwd()                   : Return atual work directory to remote commands.\n"
        "bool sshreadconfig(string)        : Load a config file (multiscript.ini like).\n"
        "                                    If string is empty look in config path.\n"
        "bool, luatable sshexec(string)    : Execute remote command in all servers of the atual group and return execution \n"
        "                                    state and a output by servers:\n"
        "                                        bool is true only if all execution is OK.\n"
        "                                        luatable layout is:\n"
        "                                            {\n"
        "                                                 serverA={\n"
        "                                                             status=int,\n"
        "                                                             {\n"
        "                                                                 stdout={line1,line2,...},\n"
        "                                                                 stderr={line1,line2,...}\n"
        "                                                             }\n"
        "                                                 },\n"
        "                                                 serverB={\n"
        "                                                             status=int,\n"
        "                                                             {\n"
        "                                                                 stdout={line1,line2,...},\n"
        "                                                                 stderr={line1,line2,...}\n"
        "                                                             }\n"
        "                                                 }\n"
        "                                            }\n"
        "                                            status is S.O. return state for executed command.\n"
        "string sshexecs(string)           : Execute remote command in all servers of the atual group and return a \n"
        "                                    formated string.\n"
        "bool, luatable sshnohup(string)   : Execute remote command in all servers of the atual group but don't wait \n"
        "                                    execution ending, returning only internal states:\n"
        "                                        bool is true only if all commands is successfully sent.\n"
        "                                        luatable layout is:\n"
        "                                            {\n"
        "                                                 serverA={\n"
        "                                                             status=int,\n"
        "                                                             {\n"
        "                                                                 stderr={line1,line2,...}\n"
        "                                                             }\n"
        "                                                 },\n"
        "                                                 serverB={\n"
        "                                                             status=int,\n"
        "                                                             {\n"
        "                                                                 stderr={line1,line2,...}\n"
        "                                                             }\n"
        "                                                 }\n"
        "                                            }\n"
        "                                            status is send command state.\n"
        "string sshnohups(string)          : Execute remote command in all servers of the atual group but don't wait\n"
        "                                    execution ending, returning only a formated string with internal states.\n"
        "string sshhelp()                  : this help.\n";
    lua_pushstring(L, s.c_str());
    return 1;
}

//========================================//

static void register_sshlib(lua_State *L)
{
    const luaL_Reg *lib;
    for (lib = sshlib; lib->func; lib++)
    {
        lua_pushcfunction(L, lib->func);
        lua_setglobal(L, lib->name);
    }
}

static void l_message (const char *pname, const char *msg)
{
    if (pname)
    {
        lua_writestringerror("%s: ", pname);
    }
    lua_writestringerror("%s\n", msg);
}

static int report(lua_State *L, int status)
{
    if (status != LUA_OK)
    {
        const char *msg = lua_tostring(L, -1);
        l_message(progname, msg);
        lua_pop(L, 1);  /* remove message */
    }
    return status;
}

static int collectargs(char **argv, int *first)
{
    int args = 0;
    int i;
    for (i = 1; argv[i] != NULL; i++)
    {
        *first = i;
        if (argv[i][0] != '-')
        {
            return args;
        }
        switch (argv[i][1])
        {
            case '-':
                if (argv[i][2] != '\0')
                {
                    return has_error;
                }
                *first = i + 1;
                return args;
            case '\0':
                return args;
            case 'E':
                if (argv[i][2] != '\0')
                {
                    return has_error;
                }
                args |= has_E;
                break;
            case 'W':
                if (argv[i][2] != '\0')
                {
                    return has_error;
                }
                break;
            case 'i':
                args |= has_i;
            case 'v':
                if (argv[i][2] != '\0')
                {
                    return has_error;
                }
                args |= has_v;
                break;
            case 'e':
                args |= has_e;
            case 'l':
                if (argv[i][2] == '\0')
                {
                    i++;  /* try next 'argv' */
                    if (argv[i] == NULL || argv[i][0] == '-')
                    {
                        return has_error;
                    }
                }
                break;
            case 'c':
                if (argv[i][2] == '\0')
                {
                    i++;  /* try next 'argv' */
                    if (argv[i] == NULL || argv[i][0] == '-')
                    {
                        return has_error;
                    }
                }
                break;
            default:
                return has_error;
        }
  }
  *first = i;
  return args;
}

static void print_usage(const char *badoption)
{
    lua_writestringerror("%s: ", progname);
    if (badoption[1] == 'e' || badoption[1] == 'l')
    {
        lua_writestringerror("'%s' needs argument\n", badoption);
    }
    else
    {
        lua_writestringerror("unrecognized option '%s'\n", badoption);
    }
    lua_writestringerror(
            "usage: %s [options] [script [args]]\n"
            "Available options are:\n"
            "  -c path  path for configuration file with group of servers list."
            "  -e stat  execute string 'stat'\n"
            "  -i       enter interactive mode after executing 'script'\n"
            "  -l name  require library 'name' into global 'name'\n"
            "  -v       show version information\n"
            "  -E       ignore environment variables\n"
            "  -W       turn warnings on\n"
            "  --       stop handling options\n"
            "  -        stop handling options and execute stdin\n"
            , progname
        );
}

static void print_version(void)
{
    std::cout << "MultiScript 0.6 based on: ";
    lua_writestring(LUA_COPYRIGHT, strlen(LUA_COPYRIGHT));
    lua_writeline();
    std::cout << std::endl << "    Type sshhelp() - To show ssh* functions !!!" << std::endl;
    lua_writeline();
}

static void createargtable(lua_State *L, char **argv, int argc, int script)
{
    int i, narg;
    if (script == argc)
    {
        script = 0;
    }
    narg = argc - (script + 1);
    lua_createtable(L, narg, script + 1);
    for (i = 0; i < argc; i++)
    {
        lua_pushstring(L, argv[i]);
        lua_rawseti(L, -2, i - script);
    }
    lua_setglobal(L, "arg");
}

static int msghandler (lua_State *L)
{
    const char *msg = lua_tostring(L, 1);
    if (msg == NULL)
    {
        if (luaL_callmeta(L, 1, "__tostring") && lua_type(L, -1) == LUA_TSTRING)
        {
            return 1;
        }
        else
        {
            msg = lua_pushfstring(L, "(error object is a %s value)", luaL_typename(L, 1));
        }
    }
    luaL_traceback(L, L, msg, 1);
    return 1;
}

static void lstop (lua_State *L, lua_Debug *ar)
{
    (void)ar;
    lua_sethook(L, NULL, 0, 0);
    luaL_error(L, "interrupted!");
}

static void laction (int i)
{
    int flag = LUA_MASKCALL | LUA_MASKRET | LUA_MASKLINE | LUA_MASKCOUNT;
    signal(i, SIG_DFL);
    lua_sethook(globalL, lstop, flag, 1);
}

static int docall(lua_State *L, int narg, int nres)
{
    int status;
    int base = lua_gettop(L) - narg;
    lua_pushcfunction(L, msghandler);
    lua_insert(L, base);
    globalL = L;
    signal(SIGINT, laction);
    status = lua_pcall(L, narg, nres, base);
    signal(SIGINT, SIG_DFL);
    lua_remove(L, base);
    return status;
}

static int dochunk(lua_State *L, int status)
{
    if (status == LUA_OK)
    {
        status = docall(L, 0, 0);
    }
    return report(L, status);
}


static int dofile(lua_State *L, const char *name)
{
    return dochunk(L, luaL_loadfile(L, name));
}


static int dostring(lua_State *L, const char *s, const char *name)
{
    return dochunk(L, luaL_loadbuffer(L, s, strlen(s), name));
}

static int dolibrary (lua_State *L, const char *name)
{
    int status;
    lua_getglobal(L, "require");
    lua_pushstring(L, name);
    status = docall(L, 1, 1);
    if (status == LUA_OK)
    {
        lua_setglobal(L, name);
    }
    return report(L, status);
}

static int handle_luainit(lua_State *L)
{
    const char *name = "=" LUA_INITVARVERSION;
    const char *init = getenv(name + 1);
    if (init == NULL)
    {
        name = "=" LUA_INIT_VAR;
        init = getenv(name + 1);  /* try alternative name */
    }
    if (init == NULL)
    {
        return LUA_OK;
    }
    else if (init[0] == '@')
    {
        return dofile(L, init+1);
    }
    else
    {
        return dostring(L, init, name);
    }
}

static int runargs (lua_State *L, char **argv, int n)
{
    int i;
    for (i = 1; i < n; i++)
    {
        int option = argv[i][1];
        lua_assert(argv[i][0] == '-');
        switch (option)
        {
        case 'e':
        case 'l':
        {
            int status;
            const char *extra = argv[i] + 2;
            if (*extra == '\0')
            {
                extra = argv[++i];
            }
            lua_assert(extra != NULL);
            status = (option == 'e') ? dostring(L, extra, "=(command line)") : dolibrary(L, extra);
            if (status != LUA_OK)
            {
                return 0;
            }
            break;
        }
        case 'c':
        {
            const char *extra = argv[i] + 2;
            if (*extra == '\0')
            {
                extra = argv[++i];
            }
            lua_assert(extra != NULL);
            SSHContext &ctx = SSHContext::Factory();
            ctx.setOtherConfigPath(extra);
            break;
        }
        case 'W':
            lua_warning(L, "@on", 0);
            break;
        default:
            break;
        }
    }
    return 1;
}

static int pushargs (lua_State *L)
{
    int i, n;
    if (lua_getglobal(L, "arg") != LUA_TTABLE)
    {
        luaL_error(L, "'arg' is not a table");
    }
    n = (int)luaL_len(L, -1);
    luaL_checkstack(L, n + 3, "too many arguments to script");
    for (i = 1; i <= n; i++)
    {
        lua_rawgeti(L, -i, i);
    }
    lua_remove(L, -i);
    return n;
}

static int handle_script (lua_State *L, char **argv)
{
    int status;
    const char *fname = argv[0];
    if (strcmp(fname, "-") == 0 && strcmp(argv[-1], "--") != 0)
    {
        fname = NULL;
    }
    status = luaL_loadfile(L, fname);
    if (status == LUA_OK)
    {
        int n = pushargs(L);
        status = docall(L, n, LUA_MULTRET);
    }
    return report(L, status);
}

static int incomplete (lua_State *L, int status)
{
    if (status == LUA_ERRSYNTAX)
    {
        size_t lmsg;
        const char *msg = lua_tolstring(L, -1, &lmsg);
        if (lmsg >= marklen && strcmp(msg + lmsg - marklen, EOFMARK) == 0)
        {
            lua_pop(L, 1);
            return 1;
        }
    }
    return 0;
}

static const char *get_prompt(lua_State *L, int firstline)
{
    if (lua_getglobal(L, firstline ? "_PROMPT" : "_PROMPT2") == LUA_TNIL)
    {
        return (firstline ? LUA_PROMPT : LUA_PROMPT2);
    }
    else
    {
        const char *p = luaL_tolstring(L, -1, NULL);
        lua_remove(L, -2);
        return p;
    }
}

static void initreadline()
{
    std::string home = Util::getHome();
    if (! home.empty())
    {
        home.append("/.multiscript");
        mkdir(home.c_str(), 0700);
        home.append("/multiscript.history");
        LuaRun::historyFile = home;
        FILE *f = fopen(LuaRun::historyFile.c_str(), "a");
        fclose(f);
        history_truncate_file(LuaRun::historyFile.c_str(), 1024);
        read_history(LuaRun::historyFile.c_str());
    }
    rl_bind_key('\t', rl_complete);
}

static int myrdline(char *_buf, const char* _prmt)
{
        char* input = readline(_prmt);
        if (!input)
        {
            return 0;
        }
        std::string s = input;
        if (! Util::trim(s).empty())
        {
            add_history(input);
            if (! LuaRun::historyFile.empty())
            {
                append_history(1, LuaRun::historyFile.c_str());
            }
        }
        strncpy(_buf, input, LUA_MAXINPUT);
        free(input);
        return -1;
}

static int pushline(lua_State *L, int firstline)
{
    char buffer[LUA_MAXINPUT];
    char *b = buffer;
    size_t l;
    const char *prmt = get_prompt(L, firstline);
    int readstatus = myrdline(b, prmt);
    if (readstatus == 0)
    {
        return 0;
    }
    lua_pop(L, 1);
    l = strlen(b);
    if (l > 0 && b[l-1] == '\n')
    {
        b[--l] = '\0';
    }
    if (firstline && b[0] == '=')
    {
        lua_pushfstring(L, "return %s", b + 1);
    }
    else
    {
        lua_pushlstring(L, b, l);
    }
    lua_freeline(L, b);
    return 1;
}

static int multiline(lua_State *L)
{
    for (;;)
    {
        size_t len;
        const char *line = lua_tolstring(L, 1, &len);
        int status = luaL_loadbuffer(L, line, len, "=stdin");
        if (!incomplete(L, status) || !pushline(L, 0))
        {
            lua_saveline(L, line);
            return status;
        }
        lua_pushliteral(L, "\n");
        lua_insert(L, -2);
        lua_concat(L, 3);
    }
}

static int addreturn(lua_State *L)
{
    const char *line = lua_tostring(L, -1);  /* original line */
    const char *retline = lua_pushfstring(L, "return %s;", line);
    int status = luaL_loadbuffer(L, retline, strlen(retline), "=stdin");
    if (status == LUA_OK)
    {
        lua_remove(L, -2);
        if (line[0] != '\0')
        {
            lua_saveline(L, line);
        }
    }
    else
    {
        lua_pop(L, 2);
    }
    return status;
}

static int loadline (lua_State *L)
{
    int status;
    lua_settop(L, 0);
    if (!pushline(L, 1))
    {
        return -1;
    }
    if ((status = addreturn(L)) != LUA_OK)
    {
        status = multiline(L);
    }
    lua_remove(L, 1);
    lua_assert(lua_gettop(L) == 1);
    return status;
}

static void l_print (lua_State *L)
{
    int n = lua_gettop(L);
    if (n > 0)
    {
        luaL_checkstack(L, LUA_MINSTACK, "too many results to print");
        lua_getglobal(L, "print");
        lua_insert(L, 1);
        if (lua_pcall(L, n, 0, 0) != LUA_OK)
        {
            l_message(progname, lua_pushfstring(L, "error calling 'print' (%s)", lua_tostring(L, -1)));
        }
    }
}

static void doREPL (lua_State *L)
{
    int status;
    const char *oldprogname = progname;
    progname = NULL;
    initreadline();
    while ((status = loadline(L)) != -1)
    {
        if (status == LUA_OK)
        {
            status = docall(L, 0, LUA_MULTRET);
        }
        if (status == LUA_OK)
        {
            l_print(L);
        }
        else
        {
            report(L, status);
        }
    }
    lua_settop(L, 0);
    lua_writeline();
    progname = oldprogname;
}

static int protected_main(lua_State *L)
{
    int argc = (int)lua_tointeger(L, 1);
    char **argv = (char **)lua_touserdata(L, 2);
    int script;
    int args = collectargs(argv, &script);
    luaL_checkversion(L);
    if (argv[0] && argv[0][0])
    {
        progname = argv[0];
    }
    if (args == has_error)
    {
        print_usage(argv[script]);
        return 0;
    }
    if (args & has_v)
    {
        print_version();
    }
    if (args & has_E)
    {
        lua_pushboolean(L, 1);  /* signal for libraries to ignore env. vars. */
        lua_setfield(L, LUA_REGISTRYINDEX, "LUA_NOENV");
    }
    luaL_openlibs(L);
        register_sshlib (L);

    createargtable(L, argv, argc, script);
    lua_gc(L, LUA_GCGEN, 0, 0);
    if (!(args & has_E))
    {
        if (handle_luainit(L) != LUA_OK)
        {
            return 0;
        }
    }
    if (!runargs(L, argv, script))
    {
        return 0;
    }
    SSHContext::Factory().readConfig();
    if (script < argc && handle_script(L, argv + script) != LUA_OK)
    {
        return 0;
    }
    if (args & has_i)
    {
        doREPL(L);
    }
    else if (script == argc && !(args & (has_e | has_v)))
    {
        if (lua_stdin_is_tty())
        {
            print_version();
            doREPL(L);
        }
        else
        {
            dofile(L, NULL);
        }
    }
    lua_pushboolean(L, 1);  /* signal no errors */
    return 1;
}

struct LuaRun::Data
{
    lua_State *L;
    Data();
    ~Data();
};

LuaRun::Data::Data()
{
    L = NULL;
    L = luaL_newstate();
}

LuaRun::Data::~Data()
{
    if (L != NULL)
    {
        lua_close(L);
        L = NULL;
    }
}

LuaRun::LuaRun()
    : d(new Data())
{
}

LuaRun::~LuaRun()
{
    delete d;
}

int LuaRun::run(int argc, char *argv[])
{
    lua_State *L = luaL_newstate();
    if (L == NULL)
    {
        std::cerr << "Cannot create state." << std::endl;
        return EXIT_FAILURE;
    }
    lua_pushcfunction(L, &protected_main);
    lua_pushinteger(L, argc);
    lua_pushlightuserdata(L, argv);
    int status = lua_pcall(L, 2, 1, 0);
    int result = lua_toboolean(L, -1);
    report(L, status);
    return (result && status == LUA_OK) ? EXIT_SUCCESS : EXIT_FAILURE;
}

