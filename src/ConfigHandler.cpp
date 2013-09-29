#include "ConfigHandler.h"
#include <iostream>
#include <fstream>
//#include <sstream>    /* included in utils.h */
//#include <algorithm>  /* included in utils.h */

//#include <functional>
//#include <cctype>
//#include <locale>
//#include <stdlib.h>
//#include <stdio.h>

#include "utils.h"

ConfigHandler::ConfigHandler()
{
    //ctor
    _configFile = "options.conf";
    _scope = scopes::s_NONE;

    _isOk = false;
    _serverMap.clear();

    loadOptions();
}

ConfigHandler::~ConfigHandler()
{
    //dtor
}

bool ConfigHandler::loadOptions()
{
    std::ifstream file(_configFile, std::ios_base::openmode::_S_in);
    if(!file.good())
    {
        std::cerr << "can't open file " << _configFile << std::endl;
        return false;
    }

    RetroShareLoadDefault(_retroShareRPCOptions.optionsMap);

    while(!file.eof())
    {
        std::string line;
        std::getline(file, line);
        processLine(line);
    }

    file.close();

    return true;
}

bool ConfigHandler::processLine(std::string& line)
{
    trim(line);

    // check for new scope
    if(switchScope(line))
        return true;

    // skipp comments / empty lines / noparse
    if(line[0] == '#' || line == "" || _scope == scopes::s_NOPARSE)
        return true;

    // parse line in key = value
    std::string key;
    std::string value;
    {
        std::istringstream is_line(line);

        if( std::getline(is_line, key, '=') )
        {
            if(! std::getline(is_line, value) )
            {
                std::cerr << "ConfigHandler::processLine() can't find a value after '=' in " << line << std::endl;
                value = "";
            }
        }
        else
        {
            std::cerr << "ConfigHandler::processLine() can't find '=' in " << line << std::endl;
            return false;
        }
    }

    trim(key);
    trim(value);

    if(key == "")
    {
        std::cerr << "ConfigHandler::processLine() empty key! value = '" << value << "'" << std::endl;
        return false;
    }

    switch(_scope)
    {
    case scopes::s_NONE:
        std::cerr << "ConfigHandler::processLine() scope NONE! not critical but still bad" << _scope << std::endl;
        break;
    case scopes::s_NOPARSE:
        break;
    case scopes::s_GENERAL:
        processScopeGeneral(key, value);
        break;
    case scopes::s_RETROSHARERPC:
        processScopeRetroShare(key, value);
        break;
    case scopes::s_AUTORESPONSE:
        processScopeAutoResponse(key, value);
        break;
    case scopes::s_AUTORESPONSE_RULE:
        processScopeAutoResponseRule(key, value);
        break;
    case scopes::s_CONTROL:
        processScopeControl(key, value);
        break;
    case scopes::s_IRC:
        processScopeIRC(key, value);
        break;
    default:
        std::cerr << "ConfigHandler::processLine() unknown scope" << _scope << std::endl;
        return false;
        break;
    }

    return true;
}

bool ConfigHandler::switchScope(std::string& line)
{
    if(line[0] == '[' && line[line.size()-1] == ']')
    {
        std::string scopeNew = line.substr(1, line.size()-2);

        if      (scopeNew == "general")
            _scope = scopes::s_GENERAL;
        else if (scopeNew == "noparse" || scopeNew == "comment")
            _scope = scopes::s_NOPARSE;
        else if (scopeNew == "retroshare")
            _scope = scopes::s_RETROSHARERPC;
        else if (scopeNew == "autoresponse")
            _scope = scopes::s_AUTORESPONSE;
        else if (scopeNew == "autoresponse-rule")
            _scope = scopes::s_AUTORESPONSE_RULE;
        else if (scopeNew == "irc")
            _scope = scopes::s_IRC;
        else if (scopeNew == "control")
            _scope = scopes::s_CONTROL;
        else
        {
            std::cerr << "ConfigHandler::processLine() unknown scope! contunuing" << scopeNew << std::endl;
            return false;
        }

        std::cout << "ConfigHandler::processLine() switched scope to " << scopeNew << std::endl;
        return true;
    }
    return false;
}

// ################## general ##################

void ConfigHandler::processScopeGeneral(std::string& key, std::string& value)
{
    if      (key == "i_version")
    {
        int ver = atoi(value.c_str());
        if(ver < 0)
            return;

        uint32_t ver2 = abs(ver);
        if(ver2 == OPTIONS_VERSION)
            _isOk = true;
    }
}

// ################## retroshare ##################

void ConfigHandler::processScopeRetroShare(std::string& key, std::string& value)
{
    //std::cout << "ConfigHandler::processScopeRetroShare() " << key.substr(0,2) << " -> " << key.substr(2, key.size()-2) << std::endl;
    if      (key == "s_address")
        _retroShareRPCOptions.address = value;
    else if (key == "i_port")
        _retroShareRPCOptions.port = atoi(value.c_str());
    else if (key == "s_user")
        _retroShareRPCOptions.userName = value;
    else if (key == "s_password")
        _retroShareRPCOptions.password = value;
    else if (key == "s_chatNickname")
        _retroShareRPCOptions.chatNickname = value;
//    else if (key == "b_autoJoin")
//        _retroShareRPCOptions.autoJoin = parseBool(value);
    else if (key == "l_autoJoinLobbies")
        processScopeRetroShareAutoJoinLobbies(value);
//    else if (key == "b_autoCreate")
//        _retroShareRPCOptions.autoCreate = parseBool(value);
    else if (key == "l_autoCreateLobbies")
        processScopeRetroShareAutoCreateLobbies(value);
//    else if (key == "b_joinAllLobbies")
//        _retroShareRPCOptions.joinAllLobbies = parseBool(value);
//    else if (key == "b_leaveLobbies")
//        _retroShareRPCOptions.leaveLobbies = parseBool(value);
//    else if (key == "b_leaveOnMsg")
//        _retroShareRPCOptions.leaveLibbyOnMsg = parseBool(value);
//    else if (key == "b_useBlacklist")
//        _retroShareRPCOptions.useLobbyBlacklist = parseBool(value);
    else if (key == "s_leaveMsg")
        _retroShareRPCOptions.leaveLobbyMsg = value;
    else if (key.substr(0,2) == "b_")
        _retroShareRPCOptions.optionsMap[
                toLower( key.substr(2, key.size()-2) )
            ] = parseBool(value);
}

void ConfigHandler::processScopeRetroShareAutoJoinLobbies(std::string& value)
{
    std::vector<std::string> lobbies = split(value, CHAT_LOBBY_SPLITTER);
    std::vector<std::string>::iterator it;
    for(it = lobbies.begin(); it != lobbies.end(); it++)
    {
        if(*it == "")
            return;

        _retroShareRPCOptions.autoJoinLobbies.push_back(trim(*it));
        std::cout << "adding lobby (join) " << *it << std::endl;
    }
}

void ConfigHandler::processScopeRetroShareAutoCreateLobbies(std::string& value)
{
    std::vector<std::string> lobbies = split(value, CHAT_LOBBY_SPLITTER);
    std::vector<std::string>::iterator it;

    for(it = lobbies.begin(); it != lobbies.end(); it++)
    {
        ConfigHandler::RetroShareRPCOptions_Lobby lobby;

        lobby.name = *it;
        it++;
        lobby.topic = *it;

        trim(lobby.name);
        trim(lobby.topic);

        if(lobby.name == "")
            continue;

        _retroShareRPCOptions.autoCreateLobbies.push_back(lobby);

        std::cout << "adding lobby (create) " << lobby.name << " (" << lobby.topic << ")" << std::endl;
    }
}

void ConfigHandler::RetroShareLoadDefault(std::map<std::string, bool>& map)
{
    // no capital letters!
    map["autocreate"] = false;
    map["autojoin"] = false;
    map["joinalllobbies"] = false;
    map["leavelobbies"] = false;
    map["leaveonmsg"] = false;
    map["useblacklist"] = false;
}

// ################## irc ##################

void ConfigHandler::processScopeIRC(std::string& key, std::string& value)
{
    if      (key == "s_server")
        processScopeIRCServer(value);
    else if (key == "s_channel")
        processScopeIRCChannel(value);
}

void ConfigHandler::processScopeIRCServer(std::string& value)
{
    std::vector<std::string> parts = split(value, IRCOptions_Splitter);
    if(parts.size() != 8)
    {
        std::cerr << "ConfigHandler::processScopeIRCServer() can't add irc server (parts size != 8)" << std::endl;
        std::cerr << "  values: " << value << std::endl;
        return;
    }

    std::string serverName = parts[0];
    IRCOptions_Server server;

    if(_serverMap.find(serverName) != _serverMap.end())
        // update?!
        server = _serverMap[serverName];
    try
    {
        server.address = trim(parts[1]);
        server.port = (uint16_t)atoi(parts[2].c_str());
        server.useSSL = parseBool(parts[3]);
        server.userName = trim(parts[4]);
        server.password = trim(parts[5]);
        server.nickName = trim(parts[6]);
        server.realName = trim(parts[7]);
    }
    catch (int e)
    {
        std::cerr << "ConfigHandler::processScopeAutoResponseValue() error parsing line:" << std::endl;
        std::cerr << "  -> " << value << std::endl;
        return;
    }

    _serverMap[serverName] = server;

    std::cout << "ConfigHandler::processScopeIRCServer() adding server (" << serverName << ") " << server.address << ":" << server.port << std::endl;
}

void ConfigHandler::processScopeIRCChannel(std::string& value)
{
    std::vector<std::string> parts = split(value, IRCOptions_Splitter);
    if(parts.size() != 4)
    {
        std::cerr << "ConfigHandler::processScopeIRCChannel() can't add irc channel (parts size != 4)" << std::endl;
        std::cerr << "  values: " << value << std::endl;
        return;
    }

    std::string serverName = parts[0];
    std::map<std::string,IRCOptions_Server>::iterator server;

    if((server = _serverMap.find(serverName)) == _serverMap.end())
    {
        std::cerr << "ConfigHandler::processScopeIRCChannel() can't find server: " << serverName << std::endl;
        return;
    }

    // s_channel = *irc channel name*;*irc channel key (optional)*;*RetroShare lobby name for the bridge*
    IRCOptions_Bridge b;

    b.ircChannelKey = trim(parts[2]);
    b.ircChannelName = trim(parts[1]);
    b.rsLobbyName = trim(parts[3]);

    server->second.channelVector.push_back(b);

    std::cout << "ConfigHandler::processScopeIRCChannel() adding channel '" << b.ircChannelName << "' bridge to '" << b.rsLobbyName << "'" << std::endl;
}

// ################## auto response ##################

void ConfigHandler::processScopeAutoResponse(std::string& key, std::string& value)
{
    if(key == "b_enable")
        _autoResponseOptions.enable = parseBool(value);
}

void ConfigHandler::processScopeAutoResponseRule(std::string& key, std::string& value)
{
    AutoResponseRule r;
    r.name = key;
    if(processScopeAutoResponseValue(value, r))
    {
        _autoResponseOptions.rules.push_back(r);

        std::cout << "adding auto reponse rule: " << r.name << std::endl;
        /*
        std::cout << "  -- module code\t" << r.usedBy << std::endl;
        std::cout << "  -- context\t\t" << r.allowContext << std::endl;
        std::cout << "  -- leading char\t" << r.hasLeadingChar << std::endl;
        std::cout << "  -- char\t\t'" << r.leadingChar << "'" << std::endl;
        std::cout << "  -- key word\t\t" << r.searchFor << std::endl;
        std::cout << "  -- answer\t\t" << r.answer << std::endl;
        */
    }
}

bool ConfigHandler::processScopeAutoResponseValue(std::string& value, ConfigHandler::AutoResponseRule& rule)
{
    try
    {
        std::vector<std::string> parts = split(value, AUTO_RESPONSE_OPTIONS_SPLITTER);

        if(parts.size() < 7)
        {
            std::cerr << "ConfigHandler::processScopeAutoResponseValue() can't add auto resonse (parts size < 7)" << std::endl;
            std::cerr << "  values: " << value << std::endl;
            return false;
        }

        for(size_t i = 0; i < parts.size(); i++)
            trim(parts[i]);

        rule.usedBy = atoi(parts[0].c_str());
        rule.allowContext = parseBool(parts[1]);
        rule.isSeparated = parseBool(parts[2]);
        rule.hasLeadingChar = parseBool(parts[3]);
        if(rule.hasLeadingChar)
        {
            if(parts[4] == "")
                return false;

            rule.leadingChar = parts[4][0];
        }
        rule.searchFor = toLower(trim(parts[5]));
        // check is msg got splitted into more parts as expected - if yes add the rest to the answer
        if(parts.size() > 7)
        {
            rule.answer = "";
            for(uint8_t i = 6; i < parts.size(); i++)
            {
                rule.answer += trim(parts[i]);
                // restore the removed splitter
                if(parts.size() > (size_t)i+1)
                    rule.answer += AUTO_RESPONSE_OPTIONS_SPLITTER;
            }
        }
        else
            rule.answer = trim(parts[6]);
    }
    catch (int e)
    {
        std::cerr << "ConfigHandler::processScopeAutoResponseValue() error parsing line:" << std::endl;
        std::cerr << "  -> " << value << std::endl;
        return false;
    }

    return true;
}

// ################## bot control ##################

void ConfigHandler::processScopeControl(std::string& key, std::string& value)
{
    if      (key == "b_enable")
        _botControlOptions.enable = parseBool(value);
    else if (key == "s_lobbyName")
        _botControlOptions.chatLobbyName = value;
    else if (key == "s_leadingChar")
        _botControlOptions.leadingChar = value[0];
}

// ################## functions ##################

bool ConfigHandler::isOk()
{
    return _isOk;
}

// ################## getters ##################

ConfigHandler::RetroShareRPCOptions& ConfigHandler::getRetroShareRPCOptions()
{
    return _retroShareRPCOptions;
}

ConfigHandler::AutoResponseOptions& ConfigHandler::getAutoResponseOptions()
{
    return _autoResponseOptions;
}

ConfigHandler::IRCOptions& ConfigHandler::getIRCOptions()
{
    _ircOptions.serverVector.clear();

    std::map<std::string,IRCOptions_Server>::iterator it;
    for(it = _serverMap.begin(); it != _serverMap.end(); it++)
        _ircOptions.serverVector.push_back(it->second);

    return _ircOptions;
}

ConfigHandler::BotControlOptions& ConfigHandler::getBotControlOptions()
{
    return _botControlOptions;
}
