#include "SSHConnector.h"
#include <iostream>

#include <stdlib.h>
#include <stdio.h>

#define DEBUG

using namespace std;

SSHConnector::SSHConnector(std::string userName, std::string password, std::string address, uint16_t port)
{
    //ctor
    _userName = userName;
    _password = password;
    _address = address;
    _port = port;
}

SSHConnector::~SSHConnector()
{
    //dtor
}

bool SSHConnector::connect()
{
    bool rc;

    rc = setupSession();
    if(rc)
        cerr << "RPC: session set up" << endl;
    else
    {
        cerr << "RPC: error setting up session" << endl;
        return false;
    }

    rc = setupChannel();
    if(rc)
        cerr << "RPC: channel set up" << endl;
    else
    {
        cerr << "RPC: error setting up channel" << endl;
        return false;
    }

    rc = setupShell();
    if(rc)
        cerr << "RPC: shell set up" << endl;
    else
    {
        cerr << "RPC: error setting up shell" << endl;
        return false;
    }

    return true;
}

bool SSHConnector::setupSession()
{
    int rc;

    // Open session and set options
    _ssh_session = ssh_new();
    if (_ssh_session == NULL)
        return false;
    ssh_options_set(_ssh_session, SSH_OPTIONS_HOST, _address.c_str());
    ssh_options_set(_ssh_session, SSH_OPTIONS_PORT, &_port);

    // Connect to server
    rc = ssh_connect(_ssh_session);
    if (rc != SSH_OK)
    {
        fprintf(stderr, "Error connecting to host %s: %s\n", _address.c_str(), ssh_get_error(_ssh_session));
        ssh_free(_ssh_session);
        return false;
    }

#if 0
    // Verify the server's identity
    // For the source code of verify_knowhost(), check previous example
    if (verify_knownhost(_ssh_session) < 0)
    {
        ssh_disconnect(_ssh_session);
        ssh_free(_ssh_session);
        return false;
    }
#endif // 0

    // Authenticate ourselves
    rc = ssh_userauth_password(_ssh_session, _userName.c_str(), _password.c_str());
    if (rc != SSH_AUTH_SUCCESS)
    {
        fprintf(stderr, "Error authenticating user '%s' with password: '%s'\n", _userName.c_str(), ssh_get_error(_ssh_session));
        ssh_disconnect(_ssh_session);
        ssh_free(_ssh_session);
        return false;
    }

    return true;
}

bool SSHConnector::setupChannel()
{
    int rc;

    // setup channel
    _ssh_channel = ssh_channel_new(_ssh_session);
    if (_ssh_channel == NULL)
        return false;

    // setup session
    rc = ssh_channel_open_session(_ssh_channel);
    if (rc != SSH_OK)
    {
        ssh_channel_free(_ssh_channel);
        return false;
    }

    return true;
}

bool SSHConnector::setupShell()
{
    int rc;

    rc = ssh_channel_request_shell(_ssh_channel);
    if(rc != SSH_OK)
        return false;

    return true;
}

void SSHConnector::disconnect()
{
    ssh_channel_close(_ssh_channel);
    ssh_disconnect(_ssh_session);
    ssh_free(_ssh_session);
}

bool SSHConnector::isConnected()
{
    return ssh_is_connected(_ssh_session);
}

ssh_channel* SSHConnector::getChannel()
{
    if (isConnected())
        return &_ssh_channel;
    return NULL;
}
