#ifndef SSHCONNECTOR_H
#define SSHCONNECTOR_H

#include <libssh/libssh.h>
//#include <iostream>
//#include <iosfwd>
#include <string>

class SSHConnector
{
public:
    SSHConnector(std::string userName, std::string password, std::string address, uint16_t port);
    virtual ~SSHConnector();

    bool connect();
    void disconnect();
    bool isConnected();
    ssh_channel* getChannel();
protected:
private:
    ssh_session _ssh_session;
    ssh_channel _ssh_channel;

    std::string _userName, _password, _address;
    uint16_t _port;

    bool setupChannel();
    bool setupSession();
    bool setupShell();
};

#endif // SSHCONNECTOR_H
