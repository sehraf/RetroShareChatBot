#include "ProtoBuf.h"

#include <iostream>
#include <limits>       // std::numeric_limits
/*
#include "gencc/core.pb.h" included in chat.pb.h
*/
#include "gencc/files.pb.h"
#include "gencc/system.pb.h"

ProtoBuf::ProtoBuf()
{
    //ctor
    _reqID = 0;
}

ProtoBuf::~ProtoBuf()
{
    //dtor
}

/*
    chat
*/
bool ProtoBuf::getRequestChatLobbiesMsg(rsctrl::chat::RequestChatLobbies::LobbySet lobbySet, ProtoBuf::RPCMessage& msg)
{
    msg.msg_id = ProtoBuf::constructMsgId(
                     (uint8_t)rsctrl::core::ExtensionId::CORE,
                     (uint16_t)rsctrl::core::PackageId::CHAT,
                     (uint8_t)rsctrl::chat::RequestMsgIds::MsgId_RequestChatLobbies,
                     false);
    msg.req_id = getRequestID();

    rsctrl::chat::RequestChatLobbies req;
    req.set_lobby_set(lobbySet);

    if(!req.SerializePartialToString(&msg.msg_body))
        return false;

    msg.msg_size = msg.msg_body.size();

    return true;
}

bool ProtoBuf::getRequestJoinOrLeaveLobbyMsg(rsctrl::chat::RequestJoinOrLeaveLobby::LobbyAction action, std::string lobbyID, ProtoBuf::RPCMessage& msg)
{
    msg.msg_id = ProtoBuf::constructMsgId(
                     (uint8_t)rsctrl::core::ExtensionId::CORE,
                     (uint16_t)rsctrl::core::PackageId::CHAT,
                     (uint8_t)rsctrl::chat::RequestMsgIds::MsgId_RequestJoinOrLeaveLobby,
                     false);
    msg.req_id = getRequestID();

    rsctrl::chat::RequestJoinOrLeaveLobby req;
    req.set_action(action);
    req.set_lobby_id(lobbyID);

    if(!req.SerializePartialToString(&msg.msg_body))
        return false;

    msg.msg_size = msg.msg_body.size();

    return true;
}

bool ProtoBuf::getRequestRegisterEventsMsg(rsctrl::chat::RequestRegisterEvents::RegisterAction action, ProtoBuf::RPCMessage& msg)
{
    msg.msg_id = ProtoBuf::constructMsgId(
                     (uint8_t)rsctrl::core::ExtensionId::CORE,
                     (uint16_t)rsctrl::core::PackageId::CHAT,
                     (uint8_t)rsctrl::chat::RequestMsgIds::MsgId_RequestRegisterEvents,
                     false);
    msg.req_id = getRequestID();

    rsctrl::chat::RequestRegisterEvents req;
    req.set_action(action);

    if(!req.SerializePartialToString(&msg.msg_body))
        return false;

    msg.msg_size = msg.msg_body.size();

    return true;
}

bool ProtoBuf::getRequestCreateLobbyMsg(std::string name, std::string topic, rsctrl::chat::LobbyPrivacyLevel provacy, ProtoBuf::RPCMessage& msg)
{
    msg.msg_id = ProtoBuf::constructMsgId(
                     (uint8_t)rsctrl::core::ExtensionId::CORE,
                     (uint16_t)rsctrl::core::PackageId::CHAT,
                     (uint8_t)rsctrl::chat::RequestMsgIds::MsgId_RequestCreateLobby,
                     false);
    msg.req_id = getRequestID();

    rsctrl::chat::RequestCreateLobby req;
    req.set_lobby_name(name);
    req.set_lobby_topic(topic);
    req.set_privacy_level(provacy);

    if(!req.SerializePartialToString(&msg.msg_body))
        return false;

    msg.msg_size = msg.msg_body.size();

    return true;
}

bool ProtoBuf::getRequestSetLobbyNicknameMsg(std::vector<std::string> ids, std::string nick, ProtoBuf::RPCMessage& msg)
{
    msg.msg_id = ProtoBuf::constructMsgId(
                     (uint8_t)rsctrl::core::ExtensionId::CORE,
                     (uint16_t)rsctrl::core::PackageId::CHAT,
                     (uint8_t)rsctrl::chat::RequestMsgIds::MsgId_RequestSetLobbyNickname,
                     false);
    msg.req_id = getRequestID();

    rsctrl::chat::RequestSetLobbyNickname req;
    std::vector<std::string>::iterator it;
    for(it = ids.begin(); it != ids.end(); it++)
        req.add_lobby_ids(*it);
    req.set_nickname(nick);

    if(!req.SerializePartialToString(&msg.msg_body))
        return false;

    msg.msg_size = msg.msg_body.size();

    return true;
}

bool ProtoBuf::getRequestSendMessageMsg(rsctrl::chat::ChatMessage& chatMsg, std::string& answerText, std::string& nick, ProtoBuf::RPCMessage& msg)
{
    return getRequestSendMessageMsg(chatMsg.id(), answerText, nick, msg);
}

bool ProtoBuf::getRequestSendMessageMsg(const rsctrl::chat::ChatId& idIn, std::string& answerText, std::string& nick, ProtoBuf::RPCMessage& msg)
{
    msg.msg_id = ProtoBuf::constructMsgId(
                     (uint8_t)rsctrl::core::ExtensionId::CORE,
                     (uint16_t)rsctrl::core::PackageId::CHAT,
                     (uint8_t)rsctrl::chat::RequestMsgIds::MsgId_RequestSendMessage,
                     false);
    msg.req_id = getRequestID();

    rsctrl::chat::RequestSendMessage req;
    rsctrl::chat::ChatMessage* msgOut = req.mutable_msg();
    rsctrl::chat::ChatId *id = msgOut->mutable_id();

    id->set_chat_type(idIn.chat_type());
    id->set_chat_id(idIn.chat_id());

    msgOut->set_msg(answerText);
    msgOut->set_peer_nickname(nick);
    msgOut->set_send_time(time(NULL));

    if(!req.SerializePartialToString(&msg.msg_body))
        return false;

    msg.msg_size = msg.msg_body.size();

    return true;
}

#ifdef ENABLE_DOWNLOAD
/*
    files
*/
bool ProtoBuf::getRequestStartDownload(std::string& fileName, std::string& fileHash, ProtoBuf::RPCMessage& msg, uint64_t fileSize /* = 0 */)
{
    msg.msg_id = ProtoBuf::constructMsgId(
            (uint8_t)rsctrl::core::ExtensionId::CORE,
            (uint16_t)rsctrl::core::PackageId::FILES,
            (uint8_t)rsctrl::files::RequestMsgIds::MsgId_RequestControlDownload,
            false);
    msg.req_id = getRequestID();

    rsctrl::files::RequestControlDownload req;
    req.set_action(rsctrl::files::RequestControlDownload_Action::RequestControlDownload_Action_ACTION_START);

    // file
    rsctrl::core::File* file = req.mutable_file();
    file->set_size(fileSize > 0 ? fileSize : 0);
    std::string* fh = new std::string(fileHash);
    std::string* fn = new std::string(fileName);
    file->set_allocated_hash(fh);
    file->set_allocated_name(fn);

    if(!req.SerializePartialToString(&msg.msg_body))
        return false;

    msg.msg_size = msg.msg_body.size();

    return true;
}
#endif // ENABLE_DOWNLOAD


/*
    help functions
*/

uint32_t ProtoBuf::getRequestID()
{
    if(_reqID == std::numeric_limits<uint32_t>::max())
        _reqID = 0;
    else
        _reqID++;

    return _reqID;
}

// Lower 8 bits.
uint8_t ProtoBuf::getRpcMsgIdSubMsg(uint32_t msg_id)
{
    return msg_id & 0xFF;
}

// Middle 16 bits.
uint16_t ProtoBuf::getRpcMsgIdService(uint32_t msg_id)
{
    return (msg_id >> 8) & 0xFFFF;
}

// Top 8 bits.
uint8_t ProtoBuf::getRpcMsgIdExtension(uint32_t msg_id)
{
    return (msg_id >> 24) & 0xFE; // Bottom Bit is for Request / Response
}

bool ProtoBuf::isRpcMsgIdResponse(uint32_t msg_id)
{
    return (msg_id >> 24) & 0x01;
}

uint32_t ProtoBuf::constructMsgId(uint8_t ext, uint16_t service, uint8_t submsg, bool is_response)
{
    if (is_response)
        ext |= 0x01; // Set Bottom Bit.
    else
        ext &= 0xFE; // Clear Bottom Bit.

    uint32_t msg_id = (ext << 24) + (service << 8) + (submsg);
    return msg_id;
}

/*
    UInt32 get/set
    taken from libretroshare\src\serialiser\rsbaseserial.cc
*/

bool getRawUInt32(void *data, uint32_t size, uint32_t *offset, uint32_t *out)
{
    /* first check there is space */
    if (size < *offset + 4)
    {
        return false;
    }
    void *buf = (void *) &(((uint8_t *) data)[*offset]);

    /* extract the data */
    uint32_t netorder_num;
    memcpy(&netorder_num, buf, sizeof(uint32_t));

    (*out) = ntohl(netorder_num);
    (*offset) += 4;
    return true;
}

bool setRawUInt32(void *data, uint32_t size, uint32_t *offset, uint32_t in)
{
    /* first check there is space */
    if (size < *offset + 4)
    {
        return false;
    }

    void *buf = (void *) &(((uint8_t *) data)[*offset]);

    /* convert the data to the right format */
    uint32_t netorder_num = htonl(in);

    /* pack it in */
    memcpy(buf, &netorder_num, sizeof(uint32_t));

    (*offset) += 4;
    return true;
}


/*
    Msg Packing
    taken from retroshare-nogui\src\rpc\rpc.cc and rpcserver.cc
*/
int MsgPacker::headersize()
{
    return kMsgHeaderSize;
}

bool MsgPacker::serialiseHeader(uint32_t msg_id, uint32_t req_id, uint32_t msg_size, uint8_t *buffer, uint32_t bufsize)
{
    /* check size */
    if (bufsize < kMsgHeaderSize)
    {
        return false;
    }

    /* pack the data (using libretroshare serialiser for now */
    void *data = buffer;
    uint32_t offset = 0;
    uint32_t size = bufsize;

    bool ok = true;

    /* 4 x uint32_t for header */
    ok &= setRawUInt32(data, size, &offset, kMsgMagicCode);
    ok &= setRawUInt32(data, size, &offset, msg_id);
    ok &= setRawUInt32(data, size, &offset, req_id);
    ok &= setRawUInt32(data, size, &offset, msg_size);

    return ok;
}

bool MsgPacker::deserialiseHeader(uint32_t &msg_id, uint32_t &req_id, uint32_t &msg_size, uint8_t *buffer, uint32_t bufsize)
{
    /* check size */
    if (bufsize < kMsgHeaderSize)
    {
        return false;
    }

    /* pack the data (using libretroshare serialiser for now */
    void *data = buffer;
    uint32_t offset = 0;
    uint32_t size = bufsize;
    uint32_t magic_code;

    bool ok = true;

    /* 4 x uint32_t for header */
    ok &= getRawUInt32(data, size, &offset, &magic_code);
    if (!ok)
    {
        std::cerr << "Failed to deserialise uint32_t(0)";
        std::cerr << std::endl;
    }
    ok &= getRawUInt32(data, size, &offset, &msg_id);
    if (!ok)
    {
        std::cerr << "Failed to deserialise uint32_t(1)";
        std::cerr << std::endl;
    }
    ok &= getRawUInt32(data, size, &offset, &req_id);
    if (!ok)
    {
        std::cerr << "Failed to deserialise uint32_t(2)";
        std::cerr << std::endl;
    }
    ok &= getRawUInt32(data, size, &offset, &msg_size);
    if (!ok)
    {
        std::cerr << "Failed to deserialise uint32_t(3)";
        std::cerr << std::endl;
    }

    ok &= (magic_code == kMsgMagicCode);
    if (!ok)
    {
        std::cerr << "Failed to Match MagicCode";
        std::cerr << std::endl;
        std::cerr << "magic_code: " << magic_code << std::endl;
        std::cerr << "msg_id: " << msg_id << std::endl;
        std::cerr << "req_id: " << req_id << std::endl;
        std::cerr << "msg_size: " << msg_size << std::endl;
    }

    return ok;
}

