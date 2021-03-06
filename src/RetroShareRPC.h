#ifndef RPC_H
#define RPC_H

#include "ChatBot.h"
#include "ProtoBuf.h"
#include "ConfigHandler.h"
#include "utils.h"
/*
#include "gencc/chat.pb.h" included in ProtoBuf.h
*/

#include <string>
#include <queue>
#include <list>
#include <thread>
#include <ctime>

/*
#include <chrono>
#include <stdint.h>
*/

class SSHConnector;
class AutoResponse;

using namespace rsctrl;

class RetroShareRPC
{
public:
    RetroShareRPC(ConfigHandler::RetroShareRPCOptions& rsopt, ConfigHandler::BotControlOptions& botcontrol, ChatBot* cb);
    virtual ~RetroShareRPC();

    bool start();
    void stop();

    void addIRCBridgeChannelName(std::string& name);

    void ircToRS(std::string& lobbyName, std::string& message);
    std::vector<std::string> getRsLobbyParticipant(std::string& lobbyName);

    void processTick();
//    void processMsgs();
protected:
private:
    SSHConnector* _ssh;
    ProtoBuf* _protobuf;
    //AutoResponse* _ar;
    ChatBot* _cb;

    const char COMMAND_SPLITTER = ';';

    bool _run;
    bool _finishQueue;
    bool _isChatRegistered;
    uint_fast32_t _tickCounter;
    std::thread _rpcWorkerThread;
    uint_fast8_t _errorCounter;
    time_t _disableAutoResponse;
    std::queue<time_t> _autoResponseHistory;

    ConfigHandler::RetroShareRPCOptions* _options;
    ConfigHandler::BotControlOptions* _botControl;
    std::queue<ProtoBuf::RPCMessage>* _rpcInQueue;
    std::queue<ProtoBuf::RPCMessage>* _rpcOutQueue;
    std::map<std::string, rsctrl::chat::ChatLobbyInfo> _lobbyMap;
    std::map<std::string, rsctrl::chat::ChatLobbyInfo> _lobbyMapBlacklist;
    std::vector<std::string>* _ircBridgeChannelNames;

    void rpcMessageLoop();

    void processChat(ProtoBuf::RPCMessage& msg);
    void processChatEvent(ProtoBuf::RPCMessage& msg);
    void processChatLobbies(ProtoBuf::RPCMessage &msg);

    void processChatMessageGeneric(chat::ChatMessage& chatmsg);
    void processChatMessageAutoResponse(rsctrl::chat::ChatMessage& chatmsg);
    void processChatMessageBotControl(rsctrl::chat::ChatMessage& chatmsg);
    void processChatMessageIRC(chat::ChatMessage& chatmsg);

    void processFile(ProtoBuf::RPCMessage& msg);

    void ircSetNick();

    void checkAutoJoinLobbies();
    void checkAutoJoinIrcLobbies();
    void checkAutoJoinLobbiesBotControl();
    void checkAutoCreatLobbies();
    void joinLeaveLobby(std::string& in, bool join, bool isID = true);
    void joinLeaveAllLobbies(bool join);

    void listCommandOptions(rsctrl::chat::ChatMessage& chatmsg);
    void listCommandLobbies(rsctrl::chat::ChatMessage& chatmsg, bool blacklist = false);
    void listCommandCommands(rsctrl::chat::ChatMessage& chatmsg);

    void setChatLobbyNick(const std::string& lobbyNameOrId, std::string& nick);
    void sendMessageToLobby(std::string& lobbyName, std::string& text);

#ifdef ENABLE_DOWNLOAD
    void startRSDownload(std::string& fileName, std::string& fileHash, uint64_t fileSize);
#endif // ENABLE_DOWNLOAD

    bool readFromChannel(ssh_channel* chan, ProtoBuf::RPCMessage& msg);
    bool writeToChannel(ssh_channel* chan, ProtoBuf::RPCMessage& msg);
};

#endif // RPC_H
