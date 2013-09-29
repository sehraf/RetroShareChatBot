#ifndef IRC_H
#define IRC_H

#include <string>
#include <thread>
#include <list>
#include <queue>
#include <map>

#include <libircclient/libircclient.h>
#include <libircclient/libirc_rfcnumeric.h>

#include "ConfigHandler.h"
#include "utils.h"

class AutoResponse;

class IRC
{
    public:
        struct ircMsg
        {
            std::string nick = "";
            std::string msg = "";
            ConfigHandler::IRCOptions_Bridge bridge;
            irc_session_t* session = NULL;
        };

        IRC(ConfigHandler::IRCOptions& ircopt, AutoResponse* ar, std::list<Utils::InterModuleCommunicationMessage>* msgList);
        virtual ~IRC();

        bool start();
        void stop();

        void processTick();
        void processMsgs();
    protected:
    private:
        static std::map<irc_session_t*, ConfigHandler::IRCOptions_Server> _sessionServerMap;
        static std::queue<ircMsg> _msgQueue;
        std::list<Utils::InterModuleCommunicationMessage>* _chatBotMsgList;
        std::thread _ircEventThread;
        ConfigHandler::IRCOptions* _options;
        AutoResponse* _ar;

        bool _run;

        irc_callbacks_t initCallbacks();
        void initSessions();

        void ircEventLoop(irc_session_t*);

        void processAutoResponse(ircMsg& msg);

        void rsToIrc(std::string& msg);
        bool rsLobbyNameToIrc(std::string& rsLobby, irc_session_t*& sessionOut, ConfigHandler::IRCOptions_Server& server, ConfigHandler::IRCOptions_Bridge& bridge);

        static void event_connect(irc_session_t * session, const char * event, const char * origin, const char ** params, unsigned int count);
        static void event_channel(irc_session_t * session, const char * event, const char * origin, const char ** params, unsigned int count);
        static void event_numeric(irc_session_t * session, unsigned int event, const char * origin, const char ** params, unsigned int count);
};

#endif // IRC_H
