#ifndef AUTORESPONSE_H
#define AUTORESPONSE_H

#include "ConfigHandler.h"

#include "gencc/chat.pb.h"

class AutoResponse
{
    public:

        static const char IRC_NEW_LINE_MARKER = '|';

        AutoResponse(ConfigHandler::AutoResponseOptions& opt);
        virtual ~AutoResponse();


        bool processMsgIrc(std::string& inMsg, std::string& inNick, std::vector<std::string>& out, std::string& ownNick);
        bool processMsgRetroShare(rsctrl::chat::ChatMessage& in, std::vector<std::string>& out, std::string& ownNick);

        void enable(bool on);
        bool isEnabled();
    protected:
    private:
        ConfigHandler::AutoResponseOptions* _options;
        std::vector<ConfigHandler::AutoResponseRule>* _rules;

        bool processMsg(std::string msg, uint16_t whatModule, std::string nick, std::string ownNick, std::vector<std::string>& out);

        void ApplyReplacements(std::string& in, std::string nick = "", std::string ownNick = "");
        void fixNewLineForIrc(std::string& in);
};

#endif // AUTORESPONSE_H
