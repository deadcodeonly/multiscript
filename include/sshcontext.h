#pragma once

#include <pthread.h>
#include <string>
#include <map>
#include <set>
#include <list>

#include <libssh/libssh.h>

class SSHContext
{
public:
    struct CommandOut
    {
        int status;
        std::list<std::string> stdout;
        std::list<std::string> stderr;
    };
    static SSHContext &Factory();
    static void destroy();
    void setOtherConfigPath(const std::string &_otherConfigPath);
    std::string getOtherConfigPath();
    void setServerGroup(const std::map< std::string, std::set<std::string> > &_serverGroup);
    std::map< std::string, std::set<std::string> > getServerGoup();
    void addServer(const std::string &_group, const std::string &_server);
    void removeServer(const std::string &_group, const std::string &_server);
    void addGroup(const std::string &_group, const std::set<std::string> &_setServer);
    void removeGroup(const std::string &_group);
    std::set<std::string> listGroup(const std::string &_group);
    void setDefaultGroup(const std::string &_group);
    std::string getDefalultGroup();
    void setDefaultPath(const std::string &_path);
    std::string getDefaultPath();
    bool readConfig(const std::string &_path = "");
    void setNohup(const bool &_nohup);
    bool getNohup();
    std::pair< bool, std::map< std::string, CommandOut > > execCommand(const std::string &_cmd);
protected:
    static pthread_mutex_t mtx;
    static SSHContext *__sshContext;
    std::string otherConfigPath;
    std::string defaultGroup;
    std::string defaultPath;
    bool nohup;
    std::map< std::string, std::set<std::string> > serverGroup;
    std::string command;
    static void *run(void *_strServer);
    SSHContext();
    virtual ~SSHContext();
    std::string getConfigPath();
    void splitStrServer(const std::string &_strServer, std::string &_user, std::string &_server, int &_port);
    ssh_session openSession(const std::string &_strServer);
    ssh_channel openChannel(ssh_session &_session);
    void runCommand(const std::string &_cmd, ssh_session &_session, ssh_channel &_channel, CommandOut *_ret);
    void closeChannel(ssh_channel &_channel);
    void closeSession(ssh_session &_session);
    void validateExecCommand(const std::string _cmd);
};
