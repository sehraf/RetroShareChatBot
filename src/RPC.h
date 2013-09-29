#ifndef RPC_H
#define RPC_H

#include "SSHConnector.h"
#include "ProtoBuf.h"

#include <string>
#include <queue>
#include <chrono>
#include <stdint.h>
#include <thread>

class RPC
{
public:
    RPC(std::string userName, std::string password, std::string address, uint16_t port);
    virtual ~RPC();

    bool start();
    void stop();

    void processTick();
    void processMsgs();

    void test();
    void test2();
protected:
private:
    SSHConnector* _ssh;
    ProtoBuf* _protobuf;
    bool _run;
    uint32_t _tickCounter;
    std::thread _rpcWorkerThread;

    std::queue<ProtoBuf::RPCMessage>* _rpcInQueue;
    std::queue<ProtoBuf::RPCMessage>* _rpcOutQueue;
    std::map<std::string, rsctrl::chat::ChatLobbyInfo>* _lobbyMap;

    void rpcMessageLoop();

    void processChat(ProtoBuf::RPCMessage& msg);

    bool readFromChannel(ssh_channel* chan, ProtoBuf::RPCMessage& msg);
    bool writeToChannel(ssh_channel* chan, ProtoBuf::RPCMessage& msg);
};

#endif // RPC_H
