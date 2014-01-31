#include "ChatBot.h"

#include <iostream>
#include <unistd.h>

#include "RetroShareRPC.h"
#include "AutoResponse.h"
#include "IRC.h"
#include "ConfigHandler.h"

ChatBot::ChatBot()
{
    //ctor
    std::cout << "ChatBot instanziert" << std::endl;

    _config = new ConfigHandler();
    // isOK() is true when the right version number was found
    // it does NOT imply that everything is fine!
    if(!_config->isOk())
    {
        std::cerr << "Error loading config! (wrong version number)" << std::endl;
        exit(EXIT_FAILURE);
    }

    ConfigHandler::AutoResponseOptions& aropt = _config->getAutoResponseOptions();
    _ar = new AutoResponse(aropt);

    ConfigHandler::BotControlOptions& botctrlopt = _config->getBotControlOptions();
    ConfigHandler::RetroShareRPCOptions& rsopt = _config->getRetroShareRPCOptions();
    // simple check
    if(rsopt.userName != "" && rsopt.address != "")
        _rpc = new RetroShareRPC(rsopt, botctrlopt, this);
    else
    {
        std::cerr << "Error seting up RPC (userName and/or address is not set)" << std::endl;
        exit(EXIT_FAILURE);
    }

    ConfigHandler::IRCOptions& ircopt = _config->getIRCOptions();
    _irc = new IRC(ircopt, this);

    // send bridged lobby names to rpc
    {
        ConfigHandler::IRCOptions_Server s;
        ConfigHandler::IRCOptions_Bridge b;
        std::vector<ConfigHandler::IRCOptions_Server>::iterator itServer;
        for(itServer = ircopt.serverVector.begin(); itServer != ircopt.serverVector.end(); itServer++)
        {
            s = *itServer;
            std::vector<ConfigHandler::IRCOptions_Bridge>::iterator itBridge;
            for(itBridge = s.channelVector.begin(); itBridge != s.channelVector.end(); itBridge++)
            {
                b = *itBridge;
                _rpc->addIRCBridgeChannelName(b.rsLobbyName);
            }
        }
    }

}

ChatBot::~ChatBot()
{
    //dtor
    std::cout << "ChatBot entfernt" << std::endl;
    delete _irc;
    delete _rpc;
    delete _config;
    delete _ar;
}

ReturnCode ChatBot::Run()
{
    std::cout << "--ChatBot::Run" << std::endl;

    bool rc;
    uint8_t counter = 0;
    _status = ReturnCode::rc_OK;

    // start subsystems
    rc = _rpc->start();
    if(!rc)
    {
        std::cerr << "ChatBot::Run() error staring RPC system" << std::endl;
        return ReturnCode::rc_ERROR;
    }
    rc = _irc->start();
    if(!rc)
    {
        std::cerr << "ChatBot::Run() error staring IRC system" << std::endl;
        return ReturnCode::rc_ERROR;
    }

    // main loop
    std::cout << "--ChatBot::Run() about to enter main loop" << std::endl;
    while (true)
    {
        _rpc->processTick();
        _irc->processTick();

        counter++;

        if(_status != ReturnCode::rc_OK)
            break;

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    // stop subsystems
    std::cout << "--ChatBot::Run() shutting down" << std::endl;
    _rpc->stop();
    _irc->stop();

    return _status;
}

void ChatBot::signalReboot()
{
    _status = ReturnCode::rc_RESTART;
}

void ChatBot::signalShutdown()
{
    _status = ReturnCode::rc_EXIT;
}
