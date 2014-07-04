#include "RetroShareRPC.h"
#include <iostream>

#include "AutoResponse.h"
#include "IRC.h"

// ################## chat process functions ##################

void RetroShareRPC::processChatMessageGeneric(chat::ChatMessage& chatmsg)
{
    std::string chatID = chatmsg.id().chat_id();

    // leave on message
    // e.g !kill
    if(_options->optionsMap["leaveonmsg"] && chatmsg.msg() == _options->leaveLobbyMsg)
    {
        joinLeaveLobby(chatID, false);

        if(_options->optionsMap["useblacklist"])
            _lobbyMapBlacklist[chatID] = _lobbyMap[chatID];
    }
}

void RetroShareRPC::processChatMessageAutoResponse(chat::ChatMessage& chatmsg)
{
    // check for time stuff
    //  -- old message?
    //  -- anti spam protection
    {
        time_t now = time(NULL);

        // auto response spam protection
        if(now <= _disableAutoResponse)
        {
            std::cout << "RetroShareRPC::processChatMessageAutoResponse() auto response disabled (spam protection)" << std::endl;
            return;
        }

        // don't answer to messages from "spam-protection-time"
        if((time_t)chatmsg.send_time() <= _disableAutoResponse)
        {
            std::cout << "RetroShareRPC::processChatMessageAutoResponse() message ignored (spam protection)" << std::endl;
            return;
        }

        // don't answer to old messages
        time_t diff = abs(now - (time_t)chatmsg.send_time());
        if(diff > 90) // 1.5 min should be enough
        {
            std::cout << "RetroShareRPC::processChatMessageAutoResponse() message ignored (too old)" << std::endl;
            std::cout << "RetroShareRPC::processChatMessageAutoResponse() now: " << now << " send_time(): " << chatmsg.send_time() <<  " -> diff: " << diff << std::endl;
            return;
        }

        // if we end up here all checks are passed
        _autoResponseHistory.push(now);
    }

    std::vector<std::string> ans;
    std::string nick = "";
    {
        // get lobby specific nick
        std::map<std::string, rsctrl::chat::ChatLobbyInfo>::iterator it;
        if((it = _lobbyMap.find(chatmsg.id().chat_id())) != _lobbyMap.end())
            nick = it->second.lobby_nickname();
    }

    if(nick == "")
        nick = _options->chatNickname; // use default

    if(_cb->_ar->processMsgRetroShare(chatmsg, ans, nick)) //chat nick is needed for replacement
    {
        std::cout << " -- answers: " << ans.size() << std::endl;

        std::vector<std::string>::iterator it;
        for(it = ans.begin(); it != ans.end(); it++)
        {
            std::cout << " -- " << *it << std::endl;

            ProtoBuf::RPCMessage msg;
            _protobuf->getRequestSendMessageMsg(chatmsg, *it, _options->chatNickname, msg);
            _rpcOutQueue->push(msg);
        }
    }
}

void RetroShareRPC::processChatMessageBotControl(chat::ChatMessage& chatmsg)
{
    if(!_botControl->enable)
        return;

    // split msg in command / parameter
    std::string command, parameter;

    size_t pos = chatmsg.msg().find(' ');
    if(pos != std::string::npos)
    {
        command = chatmsg.msg().substr(0, pos);
        parameter = chatmsg.msg().substr(pos + 1, chatmsg.msg().size() - 1);
    }
    else
    {
        command = chatmsg.msg();
        parameter = "";
    }

    trim(command);
    trim(parameter);

    toLower(command);
    // some things (like lobby names) are case sensitiv
    //toLower(parameter);

    std::cout << " -> command=" << command << " parameter=" << parameter << std::endl;

    if(command[0] != _botControl->leadingChar)
        return;

    command = command.substr(1, command.size()-1);

    // ### GENERAL ###
    if      (command == "options")
        listCommandOptions(chatmsg);
    else if (command == "commands")
        listCommandCommands(chatmsg);
    else if (command == "lobbies")
        listCommandLobbies(chatmsg);
    else if (command == "blacklist")
        listCommandLobbies(chatmsg, true);
    else if (command == "clearblacklist")
        _lobbyMapBlacklist.clear();

    // ### CHAT ###
    else if (command == "join")
    {
        if (parameter == "%all%")
            joinLeaveAllLobbies(true);
        else
            joinLeaveLobby(parameter, true, false);
    }
    else if (command == "leave")
    {
        if (parameter == "%all%")
            joinLeaveAllLobbies(false);
        else
            joinLeaveLobby(parameter, false, false);
    }
    /*
    else if (command == "autojoin")
        _options->autoJoin = parseBool(parameter);
    else if (command == "leavelobbies")
        _options->leaveLobbies = parseBool(parameter);
    else if (command == "autocreate")
        _options->autoCreate = parseBool(parameter);
    else if (command == "autojoinall")
        _options->joinAllLobbies = parseBool(parameter);
    else if (command == "leaveonmsg")
        _options->leaveLibbyOnMsg = parseBool(parameter);
    else if (command == "useblacklist")
        _options->useLobbyBlacklist = parseBool(parameter);
    */
    else if (_options->optionsMap.find(command) != _options->optionsMap.end())
        _options->optionsMap[command] = parseBool(parameter);

    // ### nick ###
    else if (command == "nick")
    {
        std::vector<std::string> parts = split(parameter, COMMAND_SPLITTER);
        if(parts.size() != 2)
            return;
        setChatLobbyNick(trim(parts[0]), trim(parts[1]));
    }
    else if (command == "say")
    {
        std::vector<std::string> parts = split(parameter, COMMAND_SPLITTER);
        if(parts.size() != 2)
            return;
        sendMessageToLobby(trim(parts[0]), trim(parts[1]));
    }

#ifdef ENABLE_DOWNLOAD
    // ### files ###
    else if (command == "download")
    {
        std::vector<std::string> parts = split(parameter, COMMAND_SPLITTER);
        if(parts.size() != 2 && parts.size() != 3)
            return;

        uint64_t fileSize = 0;
        if(parts.size() == 3)
        {
            std::istringstream iss(trim(parts[2]));
            iss >> fileSize;
        }

        //std::cout << "RetroShareRPC::processChatMessageBotControl() download request:" << std::endl
        //       << " - name: " << trim(parts[0]) << std::endl
        //        << " - hash: " << trim(parts[1]) << std::endl
        //        << " - size: " << fileSize << std::endl;

        startRSDownload(trim(parts[0]), trim(parts[1]), fileSize);
    }
#endif // ENABLE_DOWNLOAD

    // ### AR ###
    else if (command == "autoresponse")
        _cb->_ar->enable(parseBool(parameter));

    // ### CONTROL ###
    else if (command == "restart" || command == "reload")
        _cb->signalReboot();
    else if (command == "off")
        _cb->signalShutdown();
}

void RetroShareRPC::processChatMessageIRC(chat::ChatMessage& chatmsg)
{
    if(_ircBridgeChannelNames->size() == 0 || _cb->_irc == NULL)
        return;

    std::string chatID = chatmsg.id().chat_id();
    std::string msg = chatmsg.msg();

    std::vector<std::string>::iterator it;
    for(it = _ircBridgeChannelNames->begin(); it != _ircBridgeChannelNames->end(); it++)
        // (*it) is rs lobby name
        if((*it) == _lobbyMap[chatID].lobby_name())
        {
            std::string message = "<" + chatmsg.peer_nickname() + "> " + msg;

            std::cout << "RetroShareRPC::processChatMessageGeneric() sending msg to irc" << message << std::endl;

            _cb->_irc->rsToIrc((*it), message);

            // check for commands
            if(msg[0] == _botControl->leadingChar)
            {
                msg = msg.substr(1, msg.size()-1);

                //std::cout << "RetroShareRPC::processChatMessageGeneric() msg: " << msg << std::endl;
                if(msg == "list")
                {
                    _cb->_irc->requestIrcParticipant((*it));
                    std::cout << "RetroShareRPC::processChatMessageGeneric() requestion names" << std::endl;
                }
            }
        }
}

// ################## automatic things functions ##################

void RetroShareRPC::checkAutoJoinLobbies()
{
    if(!_options->optionsMap["autojoin"] || (_options->autoJoinLobbies.size() == 0 && !_options->optionsMap["joinalllobbies"]))
        return;

    // map vector to name -> true
    //std::cout << "RetroShareRPC::checkAutoJoinLobbies() lobby to join " << std::endl;
    std::map<std::string, bool> lobbiesToJoin;
    {
        std::vector<std::string>::iterator it;
        for(it = _options->autoJoinLobbies.begin(); it != _options->autoJoinLobbies.end(); it++)
        {
            //std::cout << " -- " << *it << std::endl;
            lobbiesToJoin[*it] = true;
        }
    }

    std::map<std::string, rsctrl::chat::ChatLobbyInfo>::iterator it;
    std::string id;
    std::string name;
    chat::ChatLobbyInfo lobby;
    for( it = _lobbyMap.begin(); it != _lobbyMap.end(); it++)
    {
        id = it->first;
        lobby = it->second;

        if(_options->optionsMap["useblacklist"] && _lobbyMapBlacklist.find(id) != _lobbyMapBlacklist.end())
            continue;

        name = lobby.lobby_name();
        trim(name);

        //std::cout << "RetroShareRPC::checkAutoJoinLobbies() lobby " << name << std::endl;

        if((lobbiesToJoin.find(name) != lobbiesToJoin.end() || _options->optionsMap["joinalllobbies"]) &&
                lobby.lobby_state() != chat::ChatLobbyInfo::LobbyState::ChatLobbyInfo_LobbyState_LOBBYSTATE_JOINED)
        {
            joinLeaveLobby(id, true);
            //std::cout << "RetroShareRPC::checkAutoJoinLobbies() lobby " << name << " joined" << std::endl;
        }

    }
}

void RetroShareRPC::checkAutoJoinIrcLobbies()
{
    if(_cb->_irc == NULL || _ircBridgeChannelNames->size() == 0)
        return;

    // map vector to name -> true
    //std::cout << "RetroShareRPC::checkAutoJoinIrcLobbies() lobby to join " << std::endl;
    std::map<std::string, bool> lobbiesToJoin;
    {
        std::vector<std::string>::iterator it;
        for(it = _ircBridgeChannelNames->begin(); it != _ircBridgeChannelNames->end(); it++)
        {
            //std::cout << " -- " << *it << std::endl;
            lobbiesToJoin[*it] = true;
        }
    }

    std::map<std::string, rsctrl::chat::ChatLobbyInfo>::iterator it;
    std::string id;
    std::string name;
    chat::ChatLobbyInfo lobby;
    for( it = _lobbyMap.begin(); it != _lobbyMap.end(); it++)
    {
        id = it->first;
        lobby = it->second;

        name = lobby.lobby_name();
        trim(name);

        //std::cout << "RetroShareRPC::checkAutoJoinLobbies() lobby " << name << std::endl;

        if(lobbiesToJoin.find(name) != lobbiesToJoin.end() && lobby.lobby_state() != chat::ChatLobbyInfo::LobbyState::ChatLobbyInfo_LobbyState_LOBBYSTATE_JOINED)
            joinLeaveLobby(id, true);
    }
}


void RetroShareRPC::checkAutoJoinLobbiesBotControl()
{
    if(!_botControl->enable || _botControl->chatLobbyName == "")
        return;

    std::map<std::string, rsctrl::chat::ChatLobbyInfo>::iterator it;
    std::string id;
    std::string name;
    chat::ChatLobbyInfo lobby;
    for( it = _lobbyMap.begin(); it != _lobbyMap.end(); it++)
    {
        id = it->first;
        lobby = it->second;

        name = lobby.lobby_name();
        trim(name);

        if(name == _botControl->chatLobbyName && lobby.lobby_state() != chat::ChatLobbyInfo::LobbyState::ChatLobbyInfo_LobbyState_LOBBYSTATE_JOINED)
        {
            joinLeaveLobby(id, true);
            break;
        }
    }
}

void RetroShareRPC::checkAutoCreatLobbies()
{
    if(!_options->optionsMap["autocreate"] || _options->autoCreateLobbies.size() == 0)
        return;

    // map lobbiesToCreate item  *lobby name* -> lobby item
    std::map<std::string, ConfigHandler::RetroShareRPCOptions_Lobby> lobbiesToCreate;
    {
        std::vector<ConfigHandler::RetroShareRPCOptions_Lobby>::iterator it;
        for(it = _options->autoCreateLobbies.begin(); it != _options->autoCreateLobbies.end(); it++)
            lobbiesToCreate[it->name] = *it;
    }

    // sort out lobbies we are already in or see
    {
        std::map<std::string, rsctrl::chat::ChatLobbyInfo>::iterator it;
        std::string id, name;
        chat::ChatLobbyInfo lobby;
        for( it = _lobbyMap.begin(); it != _lobbyMap.end(); it++)
        {
            id = it->first;
            lobby = it->second;

            name = lobby.lobby_name();
            trim(name);

            if(lobbiesToCreate.find(name) != lobbiesToCreate.end())
            {
                // in lobby -> remove item from lobbiesToCreate
                if(lobby.lobby_state() == chat::ChatLobbyInfo::LobbyState::ChatLobbyInfo_LobbyState_LOBBYSTATE_JOINED)
                    lobbiesToCreate.erase(lobby.lobby_name());

                // see lobby -> join
                if(lobby.lobby_state() != chat::ChatLobbyInfo::LobbyState::ChatLobbyInfo_LobbyState_LOBBYSTATE_JOINED)
                {
                    joinLeaveLobby(id, true);
                    lobbiesToCreate.erase(lobby.lobby_name());
                }
            }
        }
    }

    // iterate over the remaining item in lobbiesToCreate and create the lobbies
    {
        ConfigHandler::RetroShareRPCOptions_Lobby lobby;
        std::map<std::string, ConfigHandler::RetroShareRPCOptions_Lobby>::iterator it;
        for( it = lobbiesToCreate.begin(); it != lobbiesToCreate.end(); it++)
        {
            lobby = it->second;

            ProtoBuf::RPCMessage msg;
            _protobuf->getRequestCreateLobbyMsg(lobby.name, lobby.topic, chat::LobbyPrivacyLevel::PRIVACY_PUBLIC, msg);
            _rpcOutQueue->push(msg);
        }
    }
}

void RetroShareRPC::joinLeaveLobby(std::string& in, bool join, bool isID)
{
    std::string id;
    if(!isID)
    {
        std::map<std::string, rsctrl::chat::ChatLobbyInfo>::iterator it;
        std::string name;
        std::string id2;
        chat::ChatLobbyInfo lobby;
        for( it = _lobbyMap.begin(); it != _lobbyMap.end(); it++)
        {
            id2 = it->first;
            lobby = it->second;

            name = lobby.lobby_name();
            trim(name);

            if(name == in)
            {
                // skip bot control lobby
                if(name == _botControl->chatLobbyName)
                    continue;

                id = id2;
                break;
            }
        }
    }
    else
        id = in;

    chat::RequestJoinOrLeaveLobby::LobbyAction action;
    if (join)
        action = chat::RequestJoinOrLeaveLobby::LobbyAction::RequestJoinOrLeaveLobby_LobbyAction_JOIN_OR_ACCEPT;
    else
        action = chat::RequestJoinOrLeaveLobby::LobbyAction::RequestJoinOrLeaveLobby_LobbyAction_LEAVE_OR_DENY;

    ProtoBuf::RPCMessage msg;
    _protobuf->getRequestJoinOrLeaveLobbyMsg(action, id, msg);
    _rpcOutQueue->push(msg);
}

void RetroShareRPC::joinLeaveAllLobbies(bool join)
{
    std::map<std::string, rsctrl::chat::ChatLobbyInfo>::iterator it;
    std::string id;
    std::string name;
    chat::ChatLobbyInfo lobby;
    for( it = _lobbyMap.begin(); it != _lobbyMap.end(); it++)
    {
        id = it->first;
        lobby = it->second;

        name = lobby.lobby_name();
        trim(name);

        if(name == _botControl->chatLobbyName)
            continue;

        if      (join && lobby.lobby_state() != chat::ChatLobbyInfo::LobbyState::ChatLobbyInfo_LobbyState_LOBBYSTATE_JOINED)
            joinLeaveLobby(id, true);
        else if (!join && lobby.lobby_state() == chat::ChatLobbyInfo::LobbyState::ChatLobbyInfo_LobbyState_LOBBYSTATE_JOINED)
            joinLeaveLobby(id, false);
    }
}

// ################## help functions ##################

void RetroShareRPC::setChatLobbyNick(const std::string& lobbyNameOrId, std::string& nick)
{
    std::map<std::string, rsctrl::chat::ChatLobbyInfo>::iterator it;
    std::string id = "";

    if((it =_lobbyMap.find(lobbyNameOrId)) != _lobbyMap.end())
        id = it->first;
    else
    {
        std::string name;
        chat::ChatLobbyInfo lobby;
        for( it = _lobbyMap.begin(); it != _lobbyMap.end(); it++)
        {
            lobby = it->second;

            name = lobby.lobby_name();
            trim(name);

            if(name == lobbyNameOrId)
            {
                id = it->first;
                break;
            }
        }
    }

    // allow an empty name/id for all lobbies - but in case a name/id was set we need the lobby
    if(id == "" && lobbyNameOrId != "")
    {
        std::cerr << "RetroShareRPC::setChatLobbyNick() can't find lobby " << lobbyNameOrId << std::endl;
        return;
    }

    ProtoBuf::RPCMessage msg;
    if (id != "")
        _protobuf->getRequestSetLobbyNicknameMsg(std::vector<std::string> {id}, nick, msg);
    else
        _protobuf->getRequestSetLobbyNicknameMsg(std::vector<std::string> {}, nick, msg);
    _rpcOutQueue->push(msg);
}

void RetroShareRPC::sendMessageToLobby(std::string& lobbyName, std::string& text)
{
    std::cout << "RetroShareRPC::sendMessageToLobby() lobby name: " << lobbyName << " text: " << text << std::endl;

    // find lobby
    rsctrl::chat::ChatLobbyInfo l;
    std::string id;
    std::map<std::string, rsctrl::chat::ChatLobbyInfo>::iterator it;
    for(it = _lobbyMap.begin(); it != _lobbyMap.end(); it++)
    {
        id = it->first;
        l = it->second;

        if(l.lobby_state() == rsctrl::chat::ChatLobbyInfo::LobbyState::ChatLobbyInfo_LobbyState_LOBBYSTATE_JOINED &&
            (l.lobby_name() == lobbyName || lobbyName == "%all%"))
        {
            std::cout << "  -- sending to lobby " << l.lobby_name() << std::endl;

            // send text
            rsctrl::chat::ChatId chatId;
            chatId.set_chat_id(id);
            chatId.set_chat_type(rsctrl::chat::ChatType::TYPE_LOBBY);

            ProtoBuf::RPCMessage msg;
            _protobuf->getRequestSendMessageMsg(chatId, text, _options->chatNickname, msg);
            _rpcOutQueue->push(msg);
        }
    }
}
