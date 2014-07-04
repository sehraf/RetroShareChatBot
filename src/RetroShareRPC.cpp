#include "RetroShareRPC.h"
#include <iostream>

#include "SSHConnector.h"
#include "AutoResponse.h"
/*
#include "gencc/core.pb.h" included in chat.pb.h
#include "gencc/chat.pb.h" included in RetroShareRPC.h / ProtoBuf.h
*/
#ifdef ENABLE_DOWNLOAD
#include "gencc/files.pb.h"
#endif // ENABLE_DOWNLOAD

RetroShareRPC::RetroShareRPC(ConfigHandler::RetroShareRPCOptions& rsopt, ConfigHandler::BotControlOptions& botcontrol, ChatBot* cb)
{
    //ctor
    _ssh = new SSHConnector(rsopt.userName, rsopt.password, rsopt.address, rsopt.port);
    _protobuf = new ProtoBuf();
    _cb = cb;

    _rpcInQueue = new std::queue<ProtoBuf::RPCMessage>;
    _rpcOutQueue = new std::queue<ProtoBuf::RPCMessage>;
    _lobbyMap.clear();
    _lobbyMapBlacklist.clear();
    _ircBridgeChannelNames = new std::vector<std::string>;

    _options = &rsopt;
    _botControl = &botcontrol;
    _tickCounter = 0;
    _run = false;
    _finishQueue = false;
    _isChatRegistered = false;
    _disableAutoResponse = 0;
}

RetroShareRPC::~RetroShareRPC()
{
    std::cout << "RetroShareRPC::~RetroShareRPC()" << std::endl;

    _run = false;
    _rpcWorkerThread.join();

    //dtor
    if(_ssh->isConnected())
        _ssh->disconnect();

    delete _rpcInQueue;
    delete _rpcOutQueue;
    _lobbyMap.clear();
    _lobbyMapBlacklist.clear();

    delete _protobuf;
    delete _ssh;
}

bool RetroShareRPC::start()
{
    bool rc = 0;
    for(uint_fast8_t counter = 1; counter <= 3; counter++)
    {
        rc = _ssh->connect();
        if(!rc)
            std::cerr << "RPC::start() error connecting (" << (int_fast16_t) counter << "/3)" << std::endl;
        else
            break;

        sleep(10);
    }

    if(!rc)
    {
        std::cerr << "RPC::start() unable to connect - shuting down" << std::endl;
        return false;
    }

    _run = true;
    _finishQueue = false;
    _rpcWorkerThread = std::thread( &RetroShareRPC::rpcMessageLoop, this);
    _errorCounter = 0;

    return true;
}

void RetroShareRPC::stop()
{
    std::cout << "RetroShareRPC::stop()" << std::endl;
    //if(_options->leaveLobbies)
    if(_options->optionsMap["leavelobbies"])
    {
        std::map<std::string, rsctrl::chat::ChatLobbyInfo>::iterator it;
        std::string id;
        chat::ChatLobbyInfo lobby;
        for( it = _lobbyMap.begin(); it != _lobbyMap.end(); it++)
        {
            id = it->first;
            lobby = it->second;

            if(lobby.lobby_state() == chat::ChatLobbyInfo::LobbyState::ChatLobbyInfo_LobbyState_LOBBYSTATE_JOINED)
            {
                ProtoBuf::RPCMessage msg;
                _protobuf->getRequestJoinOrLeaveLobbyMsg(chat::RequestJoinOrLeaveLobby::LobbyAction::RequestJoinOrLeaveLobby_LobbyAction_LEAVE_OR_DENY, id, msg);
                _rpcOutQueue->push(msg);
            }
        }
    }
    _finishQueue = true;
    sleep(1);
}

void RetroShareRPC::rpcMessageLoop()
{
    ssh_channel* chan = _ssh->getChannel();
    ProtoBuf::RPCMessage msg;
    bool foundWork;

    while(_run)
    {
        foundWork = false;

        if(ssh_channel_is_closed(*chan))
        {
            std::cerr << "RPC: channel closed! :<" << std::endl;

            // restart after 10 seconds
            // if the server is still not available the bot will shutdown/crash
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            _errorCounter++;
            continue;
        }

        // read stuff
        if(ssh_channel_poll(*chan, 0) > 0)
        {
            if(readFromChannel(chan, msg))
            {
                //std::cout << "reading stuff..." << std::endl;
                _rpcInQueue->push(msg);
                foundWork = true;

                if(_errorCounter > 0)
                    _errorCounter--;
            }
            else
                _errorCounter++;
        }


        // write stuff
        if(!_rpcOutQueue->empty())
        {
            //std::cout << "sending stuff..." << std::endl;
            ProtoBuf::RPCMessage msg = _rpcOutQueue->front();
            writeToChannel(chan, msg);
            _rpcOutQueue->pop();
            foundWork = true;
        }

        if(_finishQueue && _rpcOutQueue->empty())
        {
            _run = false;
            break;
        }

        if(!_finishQueue)
        {
            if(foundWork)
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            else
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        //std::cout << "RetroShareRPC::rpcMessageLoop()" << std::endl;
    }
    std::cout << "RetroShareRPC::rpcMessageLoop() - done" << std::endl;
}

void RetroShareRPC::addIRCBridgeChannelName(std::string& name)
{
    _ircBridgeChannelNames->push_back(name);
    //_options->autoJoinLobbies.push_back(name);
}

void RetroShareRPC::processTick()
{
    // ##### timer based stuff #####
    /*
        get lobbies every 30 seconds
        (or every 5 seconds after start)
    */
    if(_tickCounter % 30 == 0 || (_tickCounter <= 15 && _tickCounter % 5 == 0))
    {
        ProtoBuf::RPCMessage msg;
        //_protobuf->getRequestChatLobbiesMsg(chat::RequestChatLobbies::LobbySet::RequestChatLobbies_LobbySet_LOBBYSET_JOINED, msg);
        _protobuf->getRequestChatLobbiesMsg(chat::RequestChatLobbies::LobbySet::RequestChatLobbies_LobbySet_LOBBYSET_ALL, msg);
        _rpcOutQueue->push(msg);
    }

    /*
        after 5 seconds set nick name
        after 45 seconds try to join lobbies
            - last lobby list request was at 30
            - next lobby list request will be at 60
        after 65 seconds create lobbies
        after 70 seconds set IRC nick if needed

        every 120 seconds try to join lobbies again
    */
    if(_tickCounter == 5)
        setChatLobbyNick("", _options->chatNickname);
    if(_tickCounter == 45 || _tickCounter % 120 == 0)
    {
        checkAutoJoinLobbies();
        checkAutoJoinIrcLobbies();
        checkAutoJoinLobbiesBotControl();
    }
    if(_tickCounter == 65)
        checkAutoCreatLobbies();

    if(_tickCounter == 70)
        ircSetNick();

    /*
        register for chat events
    */
    if(_tickCounter % 15 == 0)
    {
        if(!_isChatRegistered && _lobbyMap.size() > 0)
        {
            _isChatRegistered = true;
            ProtoBuf::RPCMessage msg;
            _protobuf->getRequestRegisterEventsMsg(chat::RequestRegisterEvents::RegisterAction::RequestRegisterEvents_RegisterAction_REGISTER, msg);
            _rpcOutQueue->push(msg);
        }
    }

    // ##### spam protection ######
    // before rpc stuff!
    // (check and disbale auto response before processing 1337 messages)
    {
        time_t now = time(NULL);

        // 5 messages in the last +30 seconds
        // disable auto repsonse for 1 min
        if(_autoResponseHistory.size() >= 5 && _disableAutoResponse < now)
        {
            std::cout << "RetroShareRPC::processTick() disabling auto response for 60 seconds" << std::endl;
            _disableAutoResponse = now + 60;
        }


        // drop old entries after 30 seconds
        // one per call is fine ( ~2 per second )
        if(_autoResponseHistory.size() > 0)
            if(_autoResponseHistory.front() < now + 30)
                _autoResponseHistory.pop();
    }

    // ##### rpc stuff #####
    ProtoBuf::RPCMessage msg;
    uint8_t counter = 0;

    // in case a lot of messages are in the queue processTick() will block the rest of the bot
    // --> leave while after max 5 loops
    while(!_rpcInQueue->empty() && counter < 5)
    {
        msg = _rpcInQueue->front();

        if(ProtoBuf::getRpcMsgIdExtension(msg.msg_id) == core::ExtensionId::CORE)
        {
            uint16_t service = ProtoBuf::getRpcMsgIdService(msg.msg_id);

            //std::cout << "RPC::processMsgs() processing service " << service << std::endl;

            switch(service)
            {
            case core::PackageId::CHAT:
                processChat(msg);
                break;
#ifdef ENABLE_DOWNLOAD
            case core::PackageId::FILES:
                processFile(msg);
                break;
#endif // ENABLE_DOWNLOAD
            default:
                std::cerr << "RPC::processMsgs() unsupportet service " << service << std::endl;
                break;
            }
            }
        else
            std::cerr << "RPC::processMsgs() extension id is not CORE" << std::endl;

        _rpcInQueue->pop();
        counter++;
    }

    // ##### error check #####
    if(_errorCounter >= 10)
        _cb->signalReboot();

    //std::cout << "RetroShareRPC::processTick() (" << _tickCounter << ")" << std::endl;
    _tickCounter++;
}

// ################## chat functions ##################

void RetroShareRPC::processChat(ProtoBuf::RPCMessage& msg)
{
    uint8_t submsg = ProtoBuf::getRpcMsgIdSubMsg(msg.msg_id);

    //std::cout << "RPC::processChat() processing submsg " << (int)submsg << std::endl;

    switch (submsg)
    {
    case chat::ResponseMsgIds::MsgId_EventChatMessage:
        //std::cout << "RPC::processChat() processing chat msg" << std::endl;
        processChatEvent(msg);
        break;
    case chat::ResponseMsgIds::MsgId_ResponseChatLobbies:
        //std::cout << "RPC::processChat() processing chat lobbies" << std::endl;
        processChatLobbies(msg);
        break;
    case chat::ResponseMsgIds::MsgId_ResponseRegisterEvents:
        //std::cout << "RPC::processChat() processing chat register event" << std::endl;
        {
            chat::ResponseRegisterEvents response;
            if(!response.ParseFromString(msg.msg_body))
            {
                std::cerr << "RPC::processChat() can't parse ResponseRegisterEvents" << std::endl;
                break;
            }
            if(response.status().code() != core::Status::SUCCESS)
            {
                std::cerr << "RPC::processChat() ResponseRegisterEvents status != SUCCESS" << std::endl;
                std::cerr << "RPC::processChat() --> msg: " << response.status().msg() << std::endl;
            }
            else
                std::cout << "RPC::processChat() ResponseRegisterEvents status == SUCCESS" << std::endl;

        }
        break;
    case chat::ResponseMsgIds::MsgId_ResponseSendMessage:
        break;
    case chat::ResponseMsgIds::MsgId_ResponseSetLobbyNickname:
        //std::cout << "RPC::processChat() processing chat set nick response" << std::endl;
        {
            chat::ResponseSetLobbyNickname response;
            if(!response.ParseFromString(msg.msg_body))
            {
                std::cerr << "RPC::processChat() can't parse ResponseSetLobbyNickname" << std::endl;
                break;
            }
            if(response.status().code() != core::Status::SUCCESS)
            {
                std::cerr << "RPC::processChat() ResponseSetLobbyNickname status != SUCCESS" << std::endl;
                std::cerr << "RPC::processChat() --> msg: " << response.status().msg() << std::endl;
            }
            else
                std::cout << "RPC::processChat() ResponseSetLobbyNickname status == SUCCESS" << std::endl;

        }
        break;
    default:
        std::cerr << "RPC::processChat() unsupportet submsg " << submsg << std::endl;
        break;
    }
}

void RetroShareRPC::processChatEvent(ProtoBuf::RPCMessage& msg)
{
    chat::EventChatMessage response;
    if(!response.ParseFromString(msg.msg_body))
    {
        std::cerr << "RPC::processChat() can't parse EventChatMessage" << std::endl;
        return;
    }

    // remove html
    chat::ChatMessage chatmsg = response.msg();
    {
        std::string tmp = chatmsg.msg();
        tmp = Utils::stripHTMLTags(tmp);
        trim(tmp);
        chatmsg.set_msg(tmp);
    }

    std::cout << " -- msg: <" << chatmsg.peer_nickname() << "> " << chatmsg.msg() << std::endl;

    // check from which lobby the message was and decide were to process
    std::string lobbyID = chatmsg.id().chat_id();
    std::map<std::string, rsctrl::chat::ChatLobbyInfo>::iterator it;
    if((it = _lobbyMap.find(lobbyID)) != _lobbyMap.end())
        if(it->second.lobby_name() == _botControl->chatLobbyName)
            processChatMessageBotControl(chatmsg);

    processChatMessageIRC(chatmsg);
    processChatMessageGeneric(chatmsg);
    processChatMessageAutoResponse(chatmsg);
}

void RetroShareRPC::processChatLobbies(ProtoBuf::RPCMessage& msg)
{
    chat::ResponseChatLobbies response;
    if(!response.ParseFromString(msg.msg_body))
    {
        std::cerr << "RPC::processChat() can't parse ResponseChatLobbies" << std::endl;
        return;
    }
    if(response.status().code() != core::Status::SUCCESS)
    {
        std::cerr << "RPC::processChat() status != SUCCESS" << std::endl;
        std::cerr << "RPC::processChat() --> msg: " << response.status().msg() << std::endl;
        return;
    }

    std::map<std::string, rsctrl::chat::ChatLobbyInfo> visibleLobbies;

    //std::cout << " -- lobbies (" << response.lobbies_size() << ")" << std::endl;
    for(int i = 0; i < response.lobbies_size(); i++)
    {
        chat::ChatLobbyInfo l = response.lobbies(i);
        //std::cout << " -- " << l.lobby_name() << "(" << l.no_peers() << ")" << std::endl;
        visibleLobbies[l.lobby_id()] = l;
    }

    std::string id;
    std::map<std::string, rsctrl::chat::ChatLobbyInfo>::iterator it;
    std::vector<std::string> idsToRemove;
    rsctrl::chat::ChatLobbyInfo lobby;

    // remove lobbies
    for(it = _lobbyMap.begin(); it != _lobbyMap.end(); it++)
    {
        id = it->first;
        if(visibleLobbies.find(id) == visibleLobbies.end())
        {
            //std::cout << "RetroShareRPC::processChatLobbies() dropping lobby " << it->second.lobby_name() << std::endl;
            idsToRemove.push_back(id);
        }
    }

    std::vector<std::string>::iterator it2;
    for(it2 = idsToRemove.begin(); it2 != idsToRemove.end(); it2++)
        _lobbyMap.erase(*it2);

    // add new / update
    for( it = visibleLobbies.begin(); it != visibleLobbies.end(); it++)
    {
        id = it->first;
        lobby = it->second;

        if(_lobbyMap.find(id) == _lobbyMap.end())
        {
            // new - do something
        }

        _lobbyMap[id] = lobby;
    }
}

// ################## chat process functions ##################
// see RetroShareChat.cpp

#ifdef ENABLE_DOWNLOAD
// ################## file functions ##################
void RetroShareRPC::processFile(ProtoBuf::RPCMessage& msg)
{
    uint8_t submsg = ProtoBuf::getRpcMsgIdSubMsg(msg.msg_id);

    //std::cout << "RPC::processFile() processing submsg " << (int)submsg << std::endl;

    switch (submsg)
    {
    case files::ResponseMsgIds::MsgId_ResponseControlDownload:
        //std::cout << "RPC::processFile() processing download controll response" << std::endl;
        {
            files::ResponseControlDownload response;
            if(!response.ParseFromString(msg.msg_body))
            {
                std::cerr << "RPC::processFile() can't parse ResponseControlDownload" << std::endl;
                break;
            }
            if(response.status().code() != core::Status::SUCCESS)
            {
                std::cerr << "RPC::processFile() ResponseControlDownload status != SUCCESS" << std::endl;
                std::cerr << "RPC::processFile() --> msg: " << response.status().msg() << std::endl;
            }
            else
                std::cout << "RPC::processFile() ResponseControlDownload status == SUCCESS" << std::endl;
        }
        break;
    case files::ResponseMsgIds::MsgId_ResponseTransferList:
        //std::cout << "RPC::processFile() processing download controll response" << std::endl;

        break;
    default:
        std::cerr << "RPC::processFile() unsupportet submsg " << submsg << std::endl;
        break;
    }
}

void RetroShareRPC::startRSDownload(std::string& fileName, std::string& fileHash, uint64_t fileSize)
{
    ProtoBuf::RPCMessage msg;
    _protobuf->getRequestStartDownload(fileName, fileHash, msg, fileSize);
    _rpcOutQueue->push(msg);
}

#endif // ENABLE_DOWNLOAD
// ################## irc functions ##################

void RetroShareRPC::ircToRS(std::string& lobbyName, std::string& message)
{
    std::cout << "RetroShareRPC::ircToRS lobby: " << lobbyName << " msg: " << message << std::endl;

    std::string name;
    std::map<std::string, chat::ChatLobbyInfo>::iterator it;
    for(it = _lobbyMap.begin(); it != _lobbyMap.end(); it++)
    {
        chat::ChatLobbyInfo info = it->second;
        name = info.lobby_name();
        trim(name);
        if(name == lobbyName)
        {
            chat::ChatMessage chatmsg;
            chat::ChatId* id = chatmsg.mutable_id();
            id->set_chat_id(it->first);
            id->set_chat_type(chat::ChatType::TYPE_LOBBY);

            ProtoBuf::RPCMessage msg;
            _protobuf->getRequestSendMessageMsg(chatmsg, message, _options->chatNickname, msg);
            _rpcOutQueue->push(msg);
        }
    }
}

void RetroShareRPC::ircSetNick()
{
    // temporary
    std::string name;
    std::string nick = "Bridge to IRC";
    std::vector<std::string>::iterator it;
    for(it = _ircBridgeChannelNames->begin(); it != _ircBridgeChannelNames->end(); it++)
    {
        name = (*it);
        setChatLobbyNick(name, nick);
    }
}

std::vector<std::string> RetroShareRPC::getRsLobbyParticipant(std::string& lobbyName)
{
    std::string name;
    chat::ChatLobbyInfo info;
    std::map<std::string, chat::ChatLobbyInfo>::iterator it;
    for(it = _lobbyMap.begin(); it != _lobbyMap.end(); it++)
    {
        info = it->second;
        name = info.lobby_name();
        trim(name);
        if(name == lobbyName)
            break;
    }

    std::vector<std::string> nameList;
    int num = info.nicknames_size();
    for(int i = 0; i < num; i++)
        nameList.push_back(info.nicknames(i));

    return nameList;
}

// ################## automatic things functions ##################
// see RetroShareChat.cpp

// ################## help functions ##################

void RetroShareRPC::listCommandOptions(chat::ChatMessage& chatmsg)
{
    std::string nl = "<br>";
    std::string out = "";

    out += "current options:" + nl;

    // chat
    out += "Chat lobbies:" + nl;
    /*
    out += " " + parseBool(_options->autoJoin) + " autojoin" + nl;
    out += " " + parseBool(_options->autoCreate) + " autocreate" + nl;
    out += " " + parseBool(_options->joinAllLobbies) +" autojoinall" + nl;
    out += " " + parseBool(_options->leaveLobbies) + " leavelobbies" + nl;
    out += " " + parseBool(_options->leaveLibbyOnMsg) + " leavelobbyonmsg" + nl;
    out += " " + parseBool(_options->useLobbyBlacklist) + " useblacklist" + nl;
    */
    std::map<std::string, bool>::iterator it;
    for(it = _options->optionsMap.begin(); it != _options->optionsMap.end(); it++)
        out += " " + parseBool(it->second) + " " + it->first + nl;

    out += nl;
    out += "Autoresponse" + nl;
    out += " " + parseBool(_cb->_ar->isEnabled()) +" enable" +  nl;

    ProtoBuf::RPCMessage msg;
    _protobuf->getRequestSendMessageMsg(chatmsg, out, _options->chatNickname, msg);
    _rpcOutQueue->push(msg);
}

void RetroShareRPC::listCommandLobbies(chat::ChatMessage& chatmsg, bool blacklist)
{
    std::string nl = "<br>";
    std::string out = "";

    if(blacklist)
        out += "blacklisted lobbies" + nl;
    else
        out += "visible lobbies:" + nl;
    out += "joined - name - topic" + nl;

    // sort lobby list
    std::multimap<std::string, rsctrl::chat::ChatLobbyInfo> tmpMMap;
    std::multimap<std::string, rsctrl::chat::ChatLobbyInfo>::iterator it;

    {
        std::map<std::string, rsctrl::chat::ChatLobbyInfo>* srcMap;
        std::map<std::string, rsctrl::chat::ChatLobbyInfo>::iterator srcIt;

        if(blacklist)
            srcMap = &_lobbyMapBlacklist;
        else
            srcMap = &_lobbyMap;

        for(srcIt = srcMap->begin(); srcIt != srcMap->end(); srcIt++)
            tmpMMap.insert( std::pair<std::string,rsctrl::chat::ChatLobbyInfo>( toLower(srcIt->second.lobby_name()), srcIt->second) );
    }

    for(it = tmpMMap.begin(); it != tmpMMap.end(); it++)
    {
        out += " "
            + parseBool(it->second.lobby_state() == chat::ChatLobbyInfo::LobbyState::ChatLobbyInfo_LobbyState_LOBBYSTATE_JOINED)
            + " - " + it->second.lobby_name();
        if(it->second.lobby_topic() == "")
            out += nl;
        else
            out += " - " + it->second.lobby_topic() + nl;
    }


    out += nl;
    out += "keep in mind that the lobby list is updated regular and might not be up to date";

    ProtoBuf::RPCMessage msg;
    _protobuf->getRequestSendMessageMsg(chatmsg, out, _options->chatNickname, msg);
    _rpcOutQueue->push(msg);
}

void RetroShareRPC::listCommandCommands(chat::ChatMessage& chatmsg)
{
    std::string nl = "<br>";
    std::string out = "";

    out += "available commands" + nl;
    out += " " + std::string(1, _botControl->leadingChar) + "commands" + nl;
    out += " " + std::string(1, _botControl->leadingChar) + "options" + nl;
    out += " " + std::string(1, _botControl->leadingChar) + "lobbies" + nl;
    out += " " + std::string(1, _botControl->leadingChar) + "blacklist" + nl;
    out += " " + std::string(1, _botControl->leadingChar) + "clearblacklist" + nl;
    out += " " + std::string(1, _botControl->leadingChar) + "nick *lobby namne*" + COMMAND_SPLITTER + " *new nick*" + nl;
    out += " " + std::string(1, _botControl->leadingChar) + "say *lobby namne*" + COMMAND_SPLITTER + " *msg*" + nl;
#ifdef ENABLE_DOWNLOAD
    out += " " + std::string(1, _botControl->leadingChar) + "download *file name*" + COMMAND_SPLITTER + " *file hash*" + "[" + COMMAND_SPLITTER + " *file size*]" + nl;
#endif // ENABLE_DOWNLOAD

    ProtoBuf::RPCMessage msg;
    _protobuf->getRequestSendMessageMsg(chatmsg, out, _options->chatNickname, msg);
    _rpcOutQueue->push(msg);
}

bool RetroShareRPC::readFromChannel(ssh_channel* chan, ProtoBuf::RPCMessage& msg)
{
    if(ssh_channel_poll(*chan, 0) == 0)
        return false;

    uint8_t buffer[kMsgHeaderSize];
    uint32_t bufsize = kMsgHeaderSize;

    uint32_t read = ssh_channel_read(*chan, buffer, bufsize, 0);

    if (read != bufsize)
    {
        std::cerr << "RetroShareRPC::readFromChannel() error reading header read=" << read << " bufsize=" << bufsize << std::endl;
        return false;
    }

    uint32_t msg_id;
    uint32_t req_id;
    uint32_t msg_size;

    if (!MsgPacker::deserialiseHeader(msg_id, req_id, msg_size, buffer, bufsize))
    {
        std::cerr << "RetroShareRPC::readFromChannel() error deserialising header" << std::endl;
        return false;
    }

    msg.msg_id = msg_id;
    msg.msg_size = msg_size;
    msg.req_id = req_id;
    msg.msg_body = "";

    char msg_body[msg_size];

    if(msg.msg_size > 0)
    {
        // wait until the complet message is available
        // wait maximal 2 seconds
        for(uint_fast8_t i = 0; i < 20; i++)
        {
            uint32_t available = ssh_channel_poll(*chan, 0);
            if(available >= msg_size)
                break;

            std::cerr << "RetroShareRPC::readFromChannel() waiting for message (" << (int)i << "/20)" << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        read = ssh_channel_read(*chan, msg_body, msg_size, 0);
        if (read != msg_size)
        {
            std::cerr << "RetroShareRPC::readFromChannel() error reading body" << std::endl;
            return false;
        }

        for(uint_fast32_t i = 0; i < msg_size; i++)
            msg.msg_body += msg_body[i];
    }
    else
        msg.msg_body = "";

//    std::cerr << "RetroShareRPC::readFromChannel()" << std::endl;
//    std::cerr << " MsgId: " << msg_id << std::endl;
//    std::cerr << " ReqId: " << req_id << std::endl;
//    std::cerr << " Body bytes: " << msg_size << "(" << msg.msg_body.size() << ")" << std::endl;

    return true;
}

bool RetroShareRPC::writeToChannel(ssh_channel* chan, ProtoBuf::RPCMessage& msg)
{
    uint8_t buffer[kMsgHeaderSize];
    uint32_t bufsize = kMsgHeaderSize;
    //uint32_t msg_size = msg.msg_size;

    if (!MsgPacker::serialiseHeader(msg.msg_id, msg.req_id, msg.msg_size, buffer, bufsize))
    {
        std::cerr << "SSHConnector::writeToChannel() error serialising header" << std::endl;
        return false;
    }

    if(!ssh_channel_write(*chan, buffer, bufsize))
    {
        std::cerr << "SSHConnector::writeToChannel() error sending header" << std::endl;
        return false;
    }

    //const char* msg_body = msg.msg_body.c_str();

    if(msg.msg_size > 0)
        if(!ssh_channel_write(*chan, msg.msg_body.c_str(), msg.msg_body.size()))
        {
            std::cerr << "SSHConnector::writeToChannel() error sending body" << std::endl;
            return false;
        }


    return true;
}
