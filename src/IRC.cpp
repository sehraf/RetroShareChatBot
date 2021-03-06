#include "IRC.h"
#include <iostream>
#include <cstring>
#include <unistd.h>

#include "AutoResponse.h"
#include "RetroShareRPC.h"

std::map<irc_session_t*, ConfigHandler::IRCOptions_Server> IRC::_sessionServerMap;
std::queue<IRC::ircMsg> IRC::_msgQueue;
IRC::ircParticipantList IRC::_ircParticipantList;
bool IRC::_suppressParticipant;

IRC::IRC(ConfigHandler::IRCOptions& ircopt, ChatBot* cb)
{
    //ctor
    _options = &ircopt;
    _cb = cb;

#ifdef DEBUG
    {
        std::cout << "IRC options:" << std::endl;

        ConfigHandler::IRCOptions_Server s;
        ConfigHandler::IRCOptions_Bridge b;
        std::vector<ConfigHandler::IRCOptions_Server>::iterator it;
        for(it = _options->serverVector.begin(); it != _options->serverVector.end(); it++)
        {
            s = *it;

            std::cout << " -- Server: " << s.address << std::endl;
            std::cout << " -- Port: " << s.port << std::endl;
            std::cout << " -- Nickname: " << s.nickName << std::endl;
            std::cout << " -- Channels: " << std::endl;

            std::vector<ConfigHandler::IRCOptions_Bridge>::iterator it2;
            for(it2 = s.channelVector.begin(); it2 != s.channelVector.end(); it2++)
            {
                b = *it2;

                std::cout << "   -- IRC Channel: " << b.ircChannelName << std::endl;
                std::cout << "   -- RS lobby: " << b.rsLobbyName << std::endl;
                std::cout << std::endl;
            }
        }
    }
#endif

    initSessions();
    _suppressParticipant = true;
}

IRC::~IRC()
{
    //dtor
}

void IRC::initSessions()
{
    IRC::_sessionServerMap.clear();
    irc_callbacks_t callbacks = initCallbacks();

    std::vector<ConfigHandler::IRCOptions_Server>::iterator it;
    for(it = _options->serverVector.begin(); it != _options->serverVector.end(); it++)
    {
        irc_session_t* s = irc_create_session(&callbacks);

        if(!s)
        {
            std::cerr << "IRC::initSessions() can't creat session for " << it->address << std::endl;
            continue;
        }
        else
            std::cout << "IRC::initSessions() created session for " << it->address << std::endl;

        irc_option_set(s, LIBIRC_OPTION_STRIPNICKS);
        IRC::_sessionServerMap[s] = *it;
    }
}

irc_callbacks_t IRC::initCallbacks()
{
    // The IRC callbacks structure
    irc_callbacks_t callbacks;

    // Init it
    memset ( &callbacks, 0, sizeof(callbacks) );

    // Set up the mandatory events
    callbacks.event_connect = event_connect;
    callbacks.event_channel = event_channel;
    callbacks.event_numeric = event_numeric;

    return callbacks;
}

bool IRC::start()
{
    if(IRC::_sessionServerMap.size() == 0)
    {
        std::cout << "IRC::start() empty server list - skipping" << std::endl;
        return true;
    }

    _run = true;

    irc_session_t* session;
    ConfigHandler::IRCOptions_Server* options;
    int32_t rc;
    uint_fast8_t failCounter = 0; // 255 should be enough
    std::thread t;

    std::map<irc_session_t*, ConfigHandler::IRCOptions_Server>::iterator it;
    for(it = IRC::_sessionServerMap.begin(); it !=IRC::_sessionServerMap.end(); it++)
    {
        session = it->first;
        options = &it->second;

        std::cout << "IRC: connecting to " << options->address << std::endl;

        // add # for ssl connection
        // todo add option
        std::string addr = "#" + options->address;

        //rc = irc_connect(session, addr.c_str(), options->port, options->password.c_str(), options->nickName.c_str(), options->userName.c_str(), options->realName.c_str());
        rc = irc_connect(session, options->address.c_str(), options->port, /*options->password.c_str()*/ NULL, options->nickName.c_str(), options->userName.c_str(), options->realName.c_str());
        if(rc) // rc != 0
        {
            std::cerr << "IRC::start() connecting to " << options->address << " failed - code: " << rc << std::endl;
            failCounter++;
        }
        else
        {
            //std::cerr << "IRC::start() connecting to " << options->address << " seems ok - code: " << rc << std::endl;
            _ircEventThread = std::thread( &IRC::ircEventLoop, this, session);

            // for now only one server!
            break;
        }
    }

    // check if every server failed to connect
    if( failCounter == IRC::_sessionServerMap.size())
        return false;

    return true;
}

void IRC::stop()
{
    // this is a mess ...

    /*
    irc_disconnect(_session);
    _ircEventThread.join();
    */

    _run = false;

    {
        std::map<irc_session_t*, ConfigHandler::IRCOptions_Server>::iterator it;
        for(it = IRC::_sessionServerMap.begin(); it !=IRC::_sessionServerMap.end(); it++)
        {
            std::cout << "IRC::stop() disconnecting " << it->second.address << std::endl;
            irc_disconnect(it->first);
        }
    }

//    {
//        std::vector<std::thread*>::iterator it;
//        for(it = _ircEventThreadVector.begin(); it != _ircEventThreadVector.end(); it++)
//            (*it)->join();
//    }
    _ircEventThread.join();


}

void IRC::ircEventLoop(irc_session_t* session)
{
    if(irc_run(session))
        irc_errno(session);

    // is this needed?
    while(_run)
    {
        sleep(1);
    }
}

void IRC::processTick()
{
    uint8_t counter = 0;

    // in case a lot of messages are in the queue processTick() will block the rest of the bot
    // --> leave while after max 5 loops
    while(!IRC::_msgQueue.empty() && counter < 5)
    {
        ircMsg ircMsg = IRC::_msgQueue.front();

#ifdef DEBUG
        std::cerr << "IRC::processTick() ircMsg:" << std::endl;
        std::cerr << " -- session: " << ircMsg.session << std::endl;
        std::cerr << " -- msg: " << ircMsg.msg << std::endl;
        std::cerr << " -- nick: " << ircMsg.nick << std::endl;
#endif

        // auto response
        if(!ircMsg.disableAutoResponse)
            processAutoResponse(ircMsg);

        processMsg(ircMsg);

        // remove item
        IRC::_msgQueue.pop();
        counter++;
    }
}

// ################## auto repose ##################

void IRC::processAutoResponse(ircMsg& ircMsg)
{
    std::cout << "IRC::processAutoResponse(): " << ircMsg.bridge.ircChannelName << std::endl;

    std::vector<std::string> ans;

    // get name for channel
    // no need for find() - if the session doesn't exist things are really bad ...
    std::string ownNick = _sessionServerMap[ircMsg.session].nickName;

    if(_cb->_ar->processMsgIrc(ircMsg.msg, ircMsg.nick, ans, ownNick)) //chat nick is needed for replacement
    {
        std::cout << " -- answers: " << ans.size() << std::endl;

        std::vector<std::string>::iterator it;
        for(it = ans.begin(); it != ans.end(); it++)
        {
            std::string msg = *it;
            std::cout << " -- " << msg << std::endl;

            std::vector<std::string> parts = split(*it, AutoResponse::IRC_NEW_LINE_MARKER);
            for(uint8_t i = 0; i < parts.size(); i++)
            {
                std::cout << "   -- splitted into: " << parts[i];
                irc_cmd_msg(ircMsg.session, ircMsg.bridge.ircChannelName.c_str(), parts[i].c_str());
            }
        }
    }
}

// ################## functions ##################

void IRC::processMsg(ircMsg& ircMsg)
{
    // send to RS
    if(_cb->_rpc != NULL)
    {
        std::string message = "<" + ircMsg.nick + "> " + ircMsg.msg;
        _cb->_rpc->ircToRS(ircMsg.bridge.rsLobbyName, message);

        if(ircMsg.msg == "!list")
        {
            std::vector<std::string> nameList = _cb->_rpc->getRsLobbyParticipant(ircMsg.bridge.rsLobbyName);

            // build answer
            const std::string seperator = ", ";
            std::string answer = "";
            answer += "People on RS side:";

            std::vector<std::string>::iterator it;
            for(it = nameList.begin(); it != nameList.end(); it++)
                answer += (*it) + seperator;

            answer.substr(0, answer.length() - seperator.length() - 1);

            irc_cmd_msg(ircMsg.session, ircMsg.bridge.ircChannelName.c_str(), answer.c_str());
        }
    }
}

void IRC::rsToIrc(std::string& lobbyName, std::string& message)
{
    std::cout << "IRC::rsToIrc() lobby: " << lobbyName << " msg: " << message << std::endl;

    // search server/channel
    irc_session_t* session = NULL;
    ConfigHandler::IRCOptions_Server s;
    ConfigHandler::IRCOptions_Bridge b;
    bool found = rsLobbyNameToIrc(lobbyName, session, s, b);

    if(found)
        irc_cmd_msg(session, b.ircChannelName.c_str(), message.c_str());
    else
        std::cerr << "IRC::rsToIrc() unable to find a fitting IRC channel (rs lobby: " << lobbyName << ")" << std::endl;
}

bool IRC::rsLobbyNameToIrc(std::string& rsLobby, irc_session_t*& sessionOut, ConfigHandler::IRCOptions_Server& server, ConfigHandler::IRCOptions_Bridge& bridge)
{
    bool found = false;
    irc_session_t* session = NULL;
    ConfigHandler::IRCOptions_Server s;
    ConfigHandler::IRCOptions_Bridge b;
    std::map<irc_session_t*, ConfigHandler::IRCOptions_Server>::iterator it;
    for(it = _sessionServerMap.begin(); it != _sessionServerMap.end(); it++)
    {
        session = it->first;
        s = it->second;
        std::vector<ConfigHandler::IRCOptions_Bridge>::iterator it2;
        for(it2 = s.channelVector.begin(); it2 != s.channelVector.end(); it2++)
        {
            b = *it2;
            if(b.rsLobbyName == rsLobby)
                found = true;

            if(found)
                break;
        }
        if(found)
            break;
    }
    if(found)
    {
        server = s;
        bridge = b;
        sessionOut = session;
        return true;
    }

    return false;
}

void IRC::requestIrcParticipant(std::string& lobbyName)
{
    std::cout << "IRC::requestIrcParticipant() lobby: " << lobbyName << std::endl;

    // participant list was requested -> remove lock
    if(_suppressParticipant)
        _suppressParticipant = false;

    // clear participants list
    _ircParticipantList.pList.clear();

    // search server/channel
    irc_session_t* session = NULL;
    ConfigHandler::IRCOptions_Server s;
    ConfigHandler::IRCOptions_Bridge b;
    bool found = rsLobbyNameToIrc(lobbyName, session, s, b);

    if(found)
        irc_cmd_names (session, b.ircChannelName.c_str());
    else
        std::cerr << "IRC::requestIrcParticipant() unable to find a fitting IRC channel (rs lobby: " << lobbyName << ")" << std::endl;
}

void IRC::progressIrcParticipant(ConfigHandler::IRCOptions_Bridge b, const std::string& names)
{
    // check whether to skip or not
    if(_suppressParticipant)
        return;

    // add people
    std::vector<std::string> nameList = split(names, ' ');
    std::vector<std::string>::iterator it;
    for(it = nameList.begin(); it != nameList.end(); it++)
        _ircParticipantList.pList.push_back(*it);

    //set bridge
    _ircParticipantList.bridge = b;
}

void IRC::progressIrcParticipantEnd()
{
    // check whether to skip or not
    if(_suppressParticipant)
        return;

    // build answer
    const std::string seperator = ", ";
    std::string answer = "";

    std::vector<std::string>::iterator it;
    for(it = _ircParticipantList.pList.begin(); it != _ircParticipantList.pList.end(); it++)
        answer += (*it) + seperator;

    answer.substr(0, answer.length() - seperator.length() - 1);

    // work is done - clear everything
    IRC::_ircParticipantList.pList.clear();

    // put in queue
    ircMsg ircMsg;

    ircMsg.msg = answer;
    ircMsg.nick = "People on IRC side";
    ircMsg.bridge = _ircParticipantList.bridge;
    ircMsg.disableAutoResponse = true;

    _msgQueue.push(ircMsg);
}

// ################## events ##################

void IRC::event_connect(irc_session_t* session, const char* event, const char* origin, const char** params, unsigned int count)
{
    std::cout << "IRC::event_connect() connected to " << IRC::_sessionServerMap[session].address << std::endl;

    std::vector<ConfigHandler::IRCOptions_Bridge> channels = IRC::_sessionServerMap[session].channelVector;
    std::vector<ConfigHandler::IRCOptions_Bridge>::iterator it;

    // set user modes
    irc_cmd_user_mode(session, "i");

    for(it = channels.begin(); it != channels.end(); it++)
    {
        std::cout << "IRC::event_connect() joining channel " << it->ircChannelName << std::endl;
        irc_cmd_join(session, it->ircChannelName.c_str(), it->ircChannelKey.c_str());
    }
}

void IRC::event_channel(irc_session_t * session, const char * event, const char * origin, const char ** params, unsigned int count)
{
    // what is this for?
    if ( !origin || count != 2 )
        return;

    char nickbuf[128];

    irc_target_get_nick (origin, nickbuf, sizeof(nickbuf));

    std::string nick = std::string(nickbuf);
    std::string msg = std::string(params[1]);


    // find channel
    bool found = false;
    ConfigHandler::IRCOptions_Bridge b;

    std::vector<ConfigHandler::IRCOptions_Bridge>::iterator it;
    for(it = _sessionServerMap[session].channelVector.begin(); it != _sessionServerMap[session].channelVector.end(); it++)
    {
        b = *it;
        if(b.ircChannelName == params[0])
        {
            found = true;
            break;
        }
    }

    if(!found)
    {
        std::cerr << "IRC::event_channel() can't find channel!" << std::endl;
        std::cerr << " -- nick: " << nickbuf << ": " << params[1] << std::endl;
        std::cerr << " -- event: " << event << std::endl;
        std::cerr << " -- origin: " << origin << std::endl;
        std::cerr << " -- params 0: " << params[0] << std::endl;
        return;
    }
#ifdef DEBUG
    else
    {
        std::cerr << "IRC::event_channel() found channel!" << std::endl;
        std::cerr << " -- nick: " << nickbuf << ": " << params[1] << std::endl;
        std::cerr << " -- event: " << event << std::endl;
        std::cerr << " -- origin: " << origin << std::endl;
        std::cerr << " -- params 0: " << params[0] << std::endl;
        std::cerr << std::endl;
        std::cerr << " -- bridge rs: " << b.rsLobbyName << std::endl;
        std::cerr << " -- bridge irc: " << b.ircChannelName << std::endl;
    }
#endif

    // put in queue
    ircMsg ircMsg;

    ircMsg.msg = msg;
    ircMsg.nick = nick;
    ircMsg.bridge = b;
    ircMsg.session = session;

    _msgQueue.push(ircMsg);
}

void IRC::event_numeric(irc_session_t* session, unsigned int event, const char* origin, const char** params, unsigned int count)
{
    if(event == LIBIRC_RFC_RPL_NAMREPLY)
    {
        //std::cout << "LIBIRC_RFC_RPL_NAMREPLY: " << params[3] << std::endl;
        // find channel
        std::string channel = params[2];
        bool found = false;
        ConfigHandler::IRCOptions_Bridge b;

        std::vector<ConfigHandler::IRCOptions_Bridge>::iterator it;
        for(it = _sessionServerMap[session].channelVector.begin(); it != _sessionServerMap[session].channelVector.end(); it++)
        {
            b = *it;
            if(b.ircChannelName == channel)
            {
                found = true;
                break;
            }
        }

        if(!found)
        {
            std::cerr << "IRC::event_numeric -> LIBIRC_RFC_RPL_NAMREPLY can't find " << std::endl;
            std::cerr << " -- channel: " << channel << std::endl;
            std::cerr << " -- names: " << params[3] << std::endl;
            return;
        }

        IRC::progressIrcParticipant(b, params[3]);
    }

    if (event == LIBIRC_RFC_RPL_ENDOFNAMES)
    {
        IRC::progressIrcParticipantEnd();
    }

    if ( event > 400 )
    {
        std::string fulltext;
        for ( unsigned int i = 0; i < count; i++ )
        {
            if ( i > 0 )
                fulltext += " ";

            fulltext += params[i];
        }

        std::cerr << "ERROR " << event << ": " << (origin ? origin : "?") << ": " << fulltext << std::endl;
        //printf ("ERROR %d: %s: %s\n", event, origin ? origin : "?", fulltext.c_str());
    }
}
