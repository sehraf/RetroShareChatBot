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

    ReturnCode Run();
protected:
private:
    RetroShareRPC* _rpc;
    ConfigHandler* _config;
    AutoResponse* _ar;
    IRC* _irc;

    std::list<Utils::InterModuleCommunicationMessage>* _messageList;

    ReturnCode processMessageList();
};

#endif // CHATBOT_H
