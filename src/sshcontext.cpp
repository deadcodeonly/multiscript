#include "sshcontext.h"

#include <unistd.h>
#include <sys/types.h>
#include <pthread.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include "mutexguard.h"
#include "util.h"

pthread_mutex_t SSHContext::mtx = PTHREAD_MUTEX_INITIALIZER;
SSHContext *SSHContext::__sshContext = NULL;

void *SSHContext::run(void *_strServer)
{
    SSHContext &ctx = SSHContext::Factory();
    std::string *str = (std::string*)_strServer;
    std::string strServer = *str;
    delete str;
    ssh_session session = NULL;
    ssh_channel channel = NULL;
    CommandOut *ret = new CommandOut();
    try
    {
        session = ctx.openSession(strServer);
        channel = ctx.openChannel(session);
        ctx.runCommand(ctx.command, session, channel, ret);
        ctx.closeChannel(channel);
        ctx.closeSession(session);
    }
    catch(const std::exception& e)
    {
        ctx.closeChannel(channel);
        ctx.closeSession(session);
        ret->status = -1;
        ret->stderr.push_back(e.what());
    }
    return (void*)ret;
}

SSHContext &SSHContext::Factory()
{
    SSHContext *ret;
    MutexGuard g(mtx);
    if (__sshContext == NULL)
    {
        __sshContext = new SSHContext();
    }
    ret = __sshContext;
    g.unlock();
    return *ret;
}

void SSHContext::destroy()
{
    MutexGuard g(mtx);
    if (__sshContext != NULL)
    {
        delete __sshContext;
        __sshContext = NULL;
    }
    g.unlock();
}

SSHContext::SSHContext()
{
    ssh_init();
    otherConfigPath = "";
    defaultPath = "~/";
}

SSHContext::~SSHContext()
{
    ssh_finalize();
}

void SSHContext::setNohup(const bool &_nohup)
{
    MutexGuard g(mtx);
    nohup = _nohup;
    g.unlock();
}

bool SSHContext::getNohup()
{
    bool ret;
    MutexGuard g(mtx);
    ret = nohup;
    g.unlock();
    return ret;
}

void SSHContext::setOtherConfigPath(const std::string &_otherConfigPath)
{
    otherConfigPath = _otherConfigPath;
}

std::string SSHContext::getOtherConfigPath()
{
    return otherConfigPath;
}

void SSHContext::setServerGroup(const std::map< std::string, std::set<std::string> > &_serverGroup)
{
    MutexGuard g(mtx);
    serverGroup.clear();
    serverGroup.insert(_serverGroup.begin(), _serverGroup.end());
    g.unlock();
}

std::map< std::string, std::set<std::string> > SSHContext::getServerGoup()
{
    MutexGuard g(mtx);
    std::map< std::string, std::set<std::string> > ret = serverGroup;
    g.unlock();
    return ret;
}

void SSHContext::addServer(const std::string &_group, const std::string &_server)
{
    if (_group.empty() || _server.empty())
    {
        return;
    }
    MutexGuard g(mtx);
    serverGroup[_group].insert(_server);
    g.unlock();
}

void SSHContext::removeServer(const std::string &_group, const std::string &_server)
{
    MutexGuard g(mtx);
    std::map< std::string, std::set<std::string> >::iterator it = serverGroup.find(_group);
    if (it != serverGroup.end())
    {
        std::set<std::string> &s = it->second;
        s.erase(_server);
    }
    g.unlock();
}

void SSHContext::addGroup(const std::string &_group, const std::set<std::string> &_setServer)
{
    if (_group.empty() || _setServer.empty())
    {
        return;
    }
    MutexGuard g(mtx);
    serverGroup[_group].insert(_setServer.begin(), _setServer.end());
    g.unlock();
}

void SSHContext::removeGroup(const std::string &_group)
{
    MutexGuard g(mtx);
    serverGroup.erase(_group);
    g.unlock();
}

std::set<std::string> SSHContext::listGroup(const std::string &_group)
{
    std::set<std::string> ret;
    MutexGuard g(mtx);
    std::map< std::string, std::set<std::string> >::iterator it = serverGroup.find(_group);
    if (it != serverGroup.end())
    {
        ret = it->second;
    }
    g.unlock();
    return ret;
}

void SSHContext::setDefaultGroup(const std::string &_group)
{
    MutexGuard g(mtx);
    defaultGroup = _group;
    g.unlock();
}

std::string SSHContext::getDefalultGroup()
{
    std::string ret;
    MutexGuard g(mtx);
    ret = defaultGroup;
    g.unlock();
    return ret;
}

void SSHContext::setDefaultPath(const std::string &_path)
{
    MutexGuard g(mtx);
    defaultPath = _path;
    g.unlock();
}

std::string SSHContext::getDefaultPath()
{
    std::string ret;
    MutexGuard g(mtx);
    ret = defaultPath;
    g.unlock();
    return ret;
}

std::pair< bool, std::map< std::string, SSHContext::CommandOut > > SSHContext::execCommand(const std::string &_cmd)
{
    std::pair< bool, std::map< std::string, CommandOut > > ret;
    validateExecCommand(_cmd);
    command = std::string("cd ") + defaultPath + " && " + _cmd;
    if (getNohup())
    {
        command.append(" 2>&1 >> nohup.out </dev/null &");
    }
    CommandOut *retcmd = new CommandOut();
    ret.first = true;
    const std::set<std::string> &servers = serverGroup[defaultGroup];
    std::list< std::pair<pthread_t, std::string> > listThread;
    for (std::set<std::string>::iterator it = servers.begin(); it != servers.end(); ++it)
    {
        std::string *server = new std::string(*it);
        pthread_t t;
        if (pthread_create(&t, NULL, run, (void*)server) != 0)
        {
            throw std::runtime_error("Error in pthread_create().");
        }
        listThread.push_back(std::make_pair(t, *server));
    }
    for (std::list< std::pair<pthread_t, std::string> >::iterator it = listThread.begin(); it != listThread.end(); ++it)
    {
        pthread_t &t = it->first;
        std::string &server = it->second;
        if (pthread_join(t, (void**)&retcmd) == 0)
        {
            if (retcmd != NULL)
            {
                ret.first = ret.first && retcmd->status == 0;
                ret.second[server] = *retcmd;
                delete retcmd;
            }
            else
            {
                CommandOut c;
                c.status = -2;
                c.stdout.push_back("Thread return NULL.");
                ret.second[server] = c;
            }
        }
        else
        {
            CommandOut c;
            c.status = -2;
            c.stderr.push_back("Error in pthread_join().");
            ret.second[server] = c;
        }
    }
    return ret;
}

bool SSHContext::readConfig(const std::string &_path)
{
    MutexGuard g(mtx);
    serverGroup.clear();
    g.unlock();
    std::string path = _path.empty() ? getConfigPath() : _path;
    if (path.empty())
    {
        return false;
    }
    std::ifstream input(path.c_str());
    if (! input.good())
    {
        return false;
    }
    std::string line;
    std::string group;
    std::set<std::string> setServer;
    std::map< std::string, std::set<std::string> > mapServer;
    bool first = true;
    while (std::getline(input, line))
    {
        Util::trim(line);
        if (line[0] == '#' || line.empty())
        {
            continue;
        }
        if (line[0] == '[' && line[line.size() -1] == ']')
        {
            if ((! group.empty()) && (! setServer.empty()))
            {
                mapServer[group] = setServer;
                setServer.clear();
            }
            group = line.substr(1, line.size() - 2);
            if (first)
            {
                defaultGroup = group;
                first = false;
            }
        }
        else
        {
            setServer.insert(line);
        }
    }
    if ((! group.empty()) && (! setServer.empty()))
    {
        mapServer[group] = setServer;
    }
    g.lock();
    serverGroup = mapServer;
    g.unlock();
    return true;
}

std::string SSHContext::getConfigPath()
{
    std::string ret;
    if (otherConfigPath.empty())
    {
        std::string home;
        MutexGuard g(mtx);
        home = Util::getHome();
        if (! home.empty())
        {
            home.append("/.multiscript/multiscript.ini");
            ret = home;
        }
    }
    else
    {
        ret = otherConfigPath;
    }
    return ret;
}

void SSHContext::splitStrServer(const std::string &_strServer, std::string &_user, std::string &_server, int &_port)
{
    std::vector<std::string> v = Util::split(_strServer, '@');
    if (v.size() > 2 || v.size() < 1)
    {
        throw std::runtime_error("Invalid format for server string.");
    }
    else if (v.size() == 2)
    {
        _user = v[0];
        v[0] = v[1];
    }
    v = Util::split(v[0], ':');
    if (v.size() > 2 || v.size() < 1)
    {
        throw std::runtime_error("Invalid format for server string.");
    }
    if (v.size() == 2)
    {
        _port = atoi(v[1].c_str());
    }
    else
    {
        _port = 22;
    }
    _server = v[0];
}

ssh_session SSHContext::openSession(const std::string &_strServer)
{
    MutexGuard g(mtx);
    std::string user;
    std::string server;
    int port;
    splitStrServer(_strServer, user, server, port);
    ssh_session sshSession = ssh_new();
    if (sshSession == NULL)
    {
        throw std::runtime_error("Error in ssh_new().");
    }
    ssh_options_set(sshSession, SSH_OPTIONS_HOST, server.c_str());
    ssh_options_set(sshSession, SSH_OPTIONS_PORT, &port);
    long stimeout = 999999999;
    ssh_options_set(sshSession, SSH_OPTIONS_TIMEOUT, &stimeout);
    unsigned int rekey = 0;
    ssh_options_set(sshSession, SSH_OPTIONS_REKEY_TIME, &rekey);
    if (! user.empty())
    {
        ssh_options_set(sshSession, SSH_OPTIONS_USER, user.c_str());
    }
    if (SSH_OK !=  ssh_connect(sshSession))
    {
        ssh_free(sshSession);
        throw std::runtime_error("Error in ssh_connect().");
    }
    if (SSH_KNOWN_HOSTS_OK != ssh_session_is_known_server(sshSession))
    {
        throw std::runtime_error("Error in ssh_session_is_known_server().");
    }
    if (SSH_AUTH_SUCCESS != ssh_userauth_publickey_auto(sshSession, NULL, NULL))
    {
        closeSession(sshSession);
        throw std::runtime_error("Error in ssh_userauth_publickey_auto().");
    }
    g.unlock();
    return sshSession;
}

ssh_channel SSHContext::openChannel(ssh_session &_session)
{
    ssh_channel channel = ssh_channel_new(_session);
    if (channel == NULL)
    {
        closeSession(_session);
        throw std::runtime_error("Error in ssh_channel_new().");
    }
    if (SSH_OK != ssh_channel_open_session(channel))
    {
        ssh_channel_free(channel);
        closeSession(_session);
        throw std::runtime_error("Error in ssh_channel_open_session().");
    }
    /*
    if (SSH_OK != ssh_channel_request_pty(channel))
    {
        closeChannel(channel);
        closeSession(_session);
        throw std::runtime_error("Error in ssh_channel_request_pty().");
    }
    if (SSH_OK != ssh_channel_change_pty_size(channel, 80, 24))
    {
        closeChannel(channel);
        closeSession(_session);
        throw std::runtime_error("Error in ssh_channel_change_pty_size().");
    }
    if (SSH_OK != ssh_channel_request_shell(channel))
    {
        closeChannel(channel);
        closeSession(_session);
        throw std::runtime_error("Error in ssh_channel_request_shell().");
    }
    */
    return channel;
}

void SSHContext::runCommand(const std::string &_cmd, ssh_session &_session, ssh_channel &_channel, CommandOut *_ret)
{
    char buffer[1024];
    std::stringstream ssok; 
    std::stringstream sser; 
    if (SSH_OK != ssh_channel_request_exec(_channel, command.c_str()))
    {
        throw std::runtime_error("Error in ssh_channel_request_exec().");
    }
    if (getNohup())
    {
        _ret->status = 0;
        return;
    }
    int status = ssh_channel_get_exit_status(_channel);
    if (SSH_ERROR == status)
    {
        throw std::runtime_error("Error in ssh_channel_get_exit_status().");
    }
    int nbytes;
    while ((nbytes = ssh_channel_read(_channel, buffer, sizeof(buffer), 0)) > 0)
    {
        std::string s = std::string(buffer, (size_t)nbytes);
        ssok << s;
    }
    if (nbytes < 0)
    {
        throw std::runtime_error("Error in ssh_channel_read(0).");
    }
    while ((nbytes = ssh_channel_read(_channel, buffer, sizeof(buffer), 1)) > 0)
    {
        std::string s = std::string(buffer, (size_t)nbytes);
        sser << s;
    }
    if (nbytes < 0)
    {
        throw std::runtime_error("Error in ssh_channel_read(1).");
    }
    _ret->status = status;
    ssok.seekg(0);
    std::string l;
    while(std::getline(ssok, l))
    {
        _ret->stdout.push_back(l);
    }
    while(std::getline(sser, l))
    {
        _ret->stderr.push_back(l);
    }
}

void SSHContext::closeChannel(ssh_channel &_channel)
{
    if (_channel != NULL)
    {
        ssh_channel_send_eof(_channel);
        ssh_channel_close(_channel);
        ssh_channel_free(_channel);
        _channel = NULL;
    }
}

void SSHContext::closeSession(ssh_session &_session)
{
    if (_session != NULL)
    {
        ssh_disconnect(_session);
        ssh_free(_session);
        _session = NULL;
    }
}

void SSHContext::validateExecCommand(const std::string _cmd)
{
    if (_cmd.empty())
    {
        throw std::runtime_error("Empty command error.");
    }
    if (getDefaultPath().empty())
    {
        throw std::runtime_error("Default path is empty, use setDefaultPath(path).");
    }
    if (getDefaultPath().empty())
    {
        throw std::runtime_error("Default group is empty, use setDefaultGroup(group).");
    }
    if (serverGroup.find(defaultGroup) == serverGroup.end())
    {
        throw std::runtime_error("Server group is empty.");
    }
}
