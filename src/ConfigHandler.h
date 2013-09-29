#ifndef CONFIGHANDLER_H
#define CONFIGHANDLER_H

//#include <iostream>
//#include <iosfwd>

#include <string>
#include <vector>
#include <map>

class ConfigHandler
{
public:
    // retroshare stuff
    struct RetroShareRPCOptions_Lobby
    {
        std::string name;
        std::string topic;
    };
    struct RetroShareRPCOptions
    {
        std::string userName = "";
        std::string password = "";
        std::string address = "";
        uint16_t port = 7022;

        std::string chatNickname = "";

        /*
        bool autoJoin = false;
        bool autoCreate = false;
        bool leaveLobbies = false;   // on shutdown
        bool joinAllLobbies = false;
        bool leaveLibbyOnMsg = false;
        bool useLobbyBlacklist = false;
        */
        std::map<std::string, bool> optionsMap;
        std::string leaveLobbyMsg = "";

        std::vector<std::string> autoJoinLobbies;
        std::vector<RetroShareRPCOptions_Lobby> autoCreateLobbies;
    };

    // auto response
    struct AutoResponseRule
    {
        std::string name = "";
        uint16_t usedBy;
        bool allowContext = false;
        bool hasLeadingChar = false;
        bool isSeparated = false;
        char leadingChar = '\0';
        std::string searchFor = "";
        std::string answer = "";
    };
    struct AutoResponseOptions
    {
        bool enable = false;
        std::vector<AutoResponseRule> rules;
    };

    // bot controll
    struct BotControlOptions
    {
        bool enable = false;
        std::string chatLobbyName = "";
        char leadingChar = '\0';
    };

    // IRC
    static const char IRCOptions_Splitter = ';';
    struct IRCOptions_Bridge
    {
        std::string ircChannelKey = "";
        std::string ircChannelName = "";
        std::string rsLobbyName = "";
    };
    struct IRCOptions_Server
    {
        std::string address = "";
        uint16_t port = 0;
        bool useSSL = false;
        std::string userName = "";
        std::string realName = "";
        std::string nickName = "";
        std::string password = "";
        std::vector<IRCOptions_Bridge> channelVector;
    };
    struct IRCOptions
    {
        std::vector<IRCOptions_Server> serverVector;
    };


    ConfigHandler();
    virtual ~ConfigHandler();

    bool isOk();

    RetroShareRPCOptions& getRetroShareRPCOptions();
    AutoResponseOptions& getAutoResponseOptions();
    BotControlOptions& getBotControlOptions();
    IRCOptions& getIRCOptions();
protected:
private:
    enum scopes
    {
        s_NONE,
        s_NOPARSE,
        s_GENERAL,
        s_RETROSHARERPC,
        s_AUTORESPONSE,
        s_AUTORESPONSE_RULE,
        s_IRC,
        s_CONTROL
    };
    scopes _scope;

    RetroShareRPCOptions _retroShareRPCOptions;
    AutoResponseOptions _autoResponseOptions;
    BotControlOptions _botControlOptions;
    IRCOptions _ircOptions;

    std::string _configFile;

    const uint32_t OPTIONS_VERSION = 1;
    const static char CHAT_LOBBY_SPLITTER = ';';
    const static char AUTO_RESPONSE_OPTIONS_SPLITTER = ';';
    bool _isOk;
    std::map<std::string,IRCOptions_Server> _serverMap;

    bool loadOptions();
    bool processLine(std::string& line);

    void processScopeGeneral(std::string& key, std::string& value);

    void processScopeRetroShare(std::string& key, std::string& value);
    void processScopeRetroShareAutoJoinLobbies(std::string& value);
    void processScopeRetroShareAutoCreateLobbies(std::string& value);
    static void RetroShareLoadDefault(std::map<std::string, bool>& map);

    void processScopeAutoResponse(std::string& key, std::string& value);
    void processScopeAutoResponseRule(std::string& key, std::string& value);
    bool processScopeAutoResponseValue(std::string& value, ConfigHandler::AutoResponseRule& rule);

    void processScopeIRC(std::string& key, std::string& value);
    void processScopeIRCServer(std::string& value);
    void processScopeIRCChannel(std::string& value);

    void processScopeControl(std::string& key, std::string& value);

    bool switchScope(std::string& line);
};

#endif // CONFIGHANDLER_H
