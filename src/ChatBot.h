#ifndef CHATBOT_H
#define CHATBOT_H

#include <list>

#include "main.h"
#include "utils.h"

class AutoResponse;
class RetroShareRPC;
class IRC;
class ConfigHandler;

class ChatBot
{
public:
    ChatBot();
    virtual ~ChatBot();

    RetroShareRPC* _rpc = NULL;
    AutoResponse* _ar = NULL;
    IRC* _irc = NULL;

    ReturnCode Run();

    void signalShutdown();
    void signalReboot();
protected:
private:
    ConfigHandler* _config;
    ReturnCode _status;

    ReturnCode processMessageList();
};

#endif // CHATBOT_H
