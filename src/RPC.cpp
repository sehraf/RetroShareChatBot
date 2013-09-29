#include "RPC.h"
#include "SSHConnector.h"
#include "ProtoBuf.h"

using namespace rsctrl;

RPC::RPC(std::string userName, std::string password, std::string address, uint16_t port)
{
    //ctor
    _ssh = new SSHConnector(userName, password, address, port);
    _protobuf = new ProtoBuf();

    _rpcInQueue = new std::queue<ProtoBuf::RPCMessage>;
    _rpcOutQueue = new std::queue<ProtoBuf::RPCMessage>;
    _lobbyMap = new std::map<std::string, rsctrl::chat::ChatLobbyInfo>;

    _tickCounter = 0;
}

RPC::~RPC()
{
    if(_run)
        stop();

    //dtor
    if(_ssh->isConnected())
        _ssh->disconnect();

    delete _rpcInQueue;
    delete _rpcOutQueue;
}

bool RPC::start()
{
    bool rc;
    rc = _ssh->connect();
    if(!rc)
    {
        std::cerr << "RPC::start: error connecting" << std::endl;
        return false;
    }

    _run = true;
    _rpcWorkerThread = std::thread( &RPC::rpcMessageLoop, this);

    return true;
}

void RPC::stop()
{
    _run = false;
    _rpcWorkerThread.join();
}

void RPC::processTick()
{
    /*
        chat lobbies
        get lobbies every 30 seconds
        (or every 5 seconds after start)
    */
    if(_tickCounter % 30 == 0 || (_tickCounter < 30 && _tickCounter % 5 == 0))
    {
        ProtoBuf::RPCMessage msg;
        _protobuf->getRequestChatLobbiesMsg(chat::RequestChatLobbies::LobbySet::RequestChatLobbies_LobbySet_LOBBYSET_ALL, msg);
        _rpcOutQueue->push(msg);
    }

    if(_tickCounter == std::numeric_limits<uint32_t>::max())
        _tickCounter = 0;
    else
        _tickCounter++;
}

void RPC::processMsgs()
{
    ProtoBuf::RPCMessage msg;

    while(!_rpcInQueue->empty())
    {
        msg = _rpcInQueue->front();

        if(ProtoBuf::getRpcMsgIdExtension(msg.msg_id) == core::ExtensionId::CORE)
        {
            uint16_t service = ProtoBuf::getRpcMsgIdService(msg.msg_id);
            switch(service)
            {
            case core::PackageId::CHAT:
                processChat(msg);
                break;
            default:
                std::cerr << "RPC::processMsgs() unsupportet servie " << service << std::endl;
                break;
            }
        }
        else
            std::cerr << "RPC::processMsgs() extension id is not CORE" << std::endl;

        _rpcInQueue->pop();
    }
}

void RPC::processChat(ProtoBuf::RPCMessage& msg)
{
    uint8_t submsg = ProtoBuf::getRpcMsgIdSubMsg(msg.msg_id);
    switch (submsg)
    {
    case chat::ResponseMsgIds::MsgId_EventChatMessage:
        std::cerr << "RPC::processChat() processing chat msg" << std::endl;
        {
            chat::EventChatMessage response;
            if(!response.ParseFromString(msg.msg_body))
            {
                std::cerr << "RPC::processChat() can't parse EventChatMessage" << std::endl;
                break;
            }
            chat::ChatMessage chatmsg = response.msg();

            std::cout << " -- msg: <" << chatmsg.peer_nickname() << "> " << chatmsg.msg() << std::endl;
        }
        break;
    case chat::ResponseMsgIds::MsgId_ResponseChatLobbies:
        std::cerr << "RPC::processChat() processing chat lobbies" << std::endl;
        {
            chat::ResponseChatLobbies response;
            if(!response.ParseFromString(msg.msg_body))
            {
                std::cerr << "RPC::processChat() can't parse ResponseChatLobbies" << std::endl;
                break;
            }
            if(response.status().code() != core::Status::SUCCESS)
            {
                std::cerr << "RPC::processChat() status != SUCCESS" << std::endl;
                break;
            }

            std::cout << " -- lobbies (" << response.lobbies_size() << ")" << std::endl;
            for(int i = 0; i < response.lobbies_size(); i++)
            {
                chat::ChatLobbyInfo l = response.lobbies(i);
                std::string id = l.lobby_id();

                std::cout << " -- " << l.lobby_name() << "(" << l.no_peers() << ")" << std::endl;

                //_lobbyMap[id] = l;

//                if(l.lobby_state() != chat::ChatLobbyInfo::LobbyState::ChatLobbyInfo_LobbyState_LOBBYSTATE_JOINED)
//                {
//                    ProtoBuf::RPCMessage msg;
//                    _protobuf->getRequestJoinOrLeaveLobbyMsg(chat::RequestJoinOrLeaveLobby::LobbyAction::RequestJoinOrLeaveLobby_LobbyAction_JOIN_OR_ACCEPT, id, msg);
//                    _rpcOutQueue->push(msg);
//                }
            }
        }
        break;
    case chat::ResponseMsgIds::MsgId_ResponseRegisterEvents:
        break;
    case chat::ResponseMsgIds::MsgId_ResponseSendMessage:
        break;
    case chat::ResponseMsgIds::MsgId_ResponseSetLobbyNickname:
        break;
    default:
        std::cerr << "RPC::processChat() unsupportet submsg " << submsg << std::endl;
        break;
    }
}

void RPC::rpcMessageLoop()
{
    ssh_channel* chan = _ssh->getChannel();
    ProtoBuf::RPCMessage msg;

    while(_run)
    {
        if(ssh_channel_is_closed(*chan))
        {
            std::cout << "RPC: channel closed! :<" << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            continue;
        }

        // read stuff
        //if(!ssh_channel_(*chan))
        if(readFromChannel(chan, msg))
        {
            std::cerr << "reading stuff..." << std::endl;
            _rpcInQueue->push(msg);
        }

        // write stuff
        if(!_rpcOutQueue->empty())
        {
            std::cerr << "sending stuff..." << std::endl;
            ProtoBuf::RPCMessage msg = _rpcOutQueue->front();
            writeToChannel(chan, msg);
            _rpcOutQueue->pop();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    return;
}

bool RPC::readFromChannel(ssh_channel* chan, ProtoBuf::RPCMessage& msg)
{
    uint8_t buffer[kMsgHeaderSize];
    uint32_t bufsize = kMsgHeaderSize;

    uint32_t read = ssh_channel_read_nonblocking(*chan, buffer, bufsize, 0);
    if(read == 0)
        return false;

    if (read != bufsize)
    {

        std::cerr << "SSHConnector::readFromChannel() error reading header read=" << read << " bufsize=" << bufsize << std::endl;
        return false;
    }

    uint32_t msg_id;
    uint32_t req_id;
    uint32_t msg_size;

    if (!MsgPacker::deserialiseHeader(msg_id, req_id, msg_size, buffer, bufsize))
    {
        std::cerr << "SSHConnector::readFromChannel() error deserialising header" << std::endl;
        return false;
    }

    msg.msg_id = msg_id;
    msg.msg_size = msg_size;
    msg.req_id = req_id;
    msg.msg_body = "";

    char msg_body[msg_size];

    if(msg.msg_size > 0)
    {
        read = ssh_channel_read(*chan, msg_body, msg_size, 0);
        if (read != msg_size)
        {
            std::cerr << "SSHConnector::readFromChannel() error reading body" << std::endl;
            return false;
        }
        for(uint_fast32_t i = 0; i < msg_size; i++)
            msg.msg_body += msg_body[i];
    }
    else
        msg.msg_body = "";

    std::cerr << "SSHConnector::readFromChannel()" << std::endl;
    std::cerr << " MsgId: " << msg_id << std::endl;
    std::cerr << " ReqId: " << req_id << std::endl;
    std::cerr << " Body bytes: " << msg_size << "(" << msg.msg_body.size() << ")" << std::endl;

    return true;
}

bool RPC::writeToChannel(ssh_channel* chan, ProtoBuf::RPCMessage& msg)
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

//    string tmp;
//    for(uint32_t i = 0; i < kMsgHeaderSize; i++)
//        tmp += buffer[i];
//    tmp += msg.msg_body;
//
//    if(!ssh_channel_write(_ssh_channel, tmp.c_str(), tmp.size()))
//    {
//        std::cerr << "SSHConnector::writeToChannel() error sending all" << std::endl;
//        return false;
//    }

    //ssh_blocking_flush(_ssh_session, 500);

    return true;
}

/*
void RPC::test()
{
    ProtoBuf::RPCMessage msg;

    system::RequestSystemStatus req;

    msg.msg_id = ProtoBuf::constructMsgId((uint8_t)core::ExtensionId::CORE, (uint16_t)core::PackageId::SYSTEM, (uint8_t)system::RequestMsgIds::MsgId_RequestSystemStatus, false);
//    msg.req_id = _protobuf->getRequestID();
    if(!req.SerializePartialToString(&msg.msg_body))
    {
        return;
    }
    msg.msg_size = msg.msg_body.size();

//    std::cerr << " MsgId: " << msg.msg_id << std::endl;
//    std::cerr << " ReqId: " << msg.req_id << std::endl;
//    std::cerr << " Body bytes: " << msg.msg_size << std::endl;

    _rpcOutQueue->push(msg);
}

void RPC::test2()
{
    ProtoBuf::RPCMessage msg;
    if(_rpcInQueue->empty())
    {
        std::cerr << "empty queue" << std::endl;
        return;
    }

    msg = _rpcInQueue->front();

    if(!ProtoBuf::isRpcMsgIdResponse(msg.msg_id))
    {
        std::cerr << "no response" << std::endl;
        return;
    }

    if(ProtoBuf::getRpcMsgIdExtension(msg.msg_id) == core::ExtensionId::CORE
            && ProtoBuf::getRpcMsgIdService(msg.msg_id) == core::PackageId::SYSTEM
            && ProtoBuf::getRpcMsgIdSubMsg(msg.msg_id) == system::ResponseMsgIds::MsgId_ResponseSystemStatus)
    {
        system::ResponseSystemStatus res;
        res.ParseFromString(msg.msg_body);
        std::cerr << "--> system status" << std::endl;
        std::cerr << " bw: " << res.bw_total().down() << std::endl;
        std::cerr << " peers: " << res.no_peers() << std::endl;

    }
    else
    {
        std::cerr << "wrong msg id" << std::endl;
        return;
    }
}
*/
