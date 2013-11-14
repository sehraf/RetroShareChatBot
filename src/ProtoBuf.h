#ifndef PROTOBUF_H
#define PROTOBUF_H

#include <libssh/libssh.h>
#include <string>
#include <vector>

#include "gencc/chat.pb.h"

const uint32_t kMsgHeaderSize = 16;
const uint32_t kMsgMagicCode  = 0x137f0001;

bool getRawUInt32(void *data, uint32_t size, uint32_t *offset, uint32_t *out);
bool setRawUInt32(void *data, uint32_t size, uint32_t *offset, uint32_t in);

class ProtoBuf
{
public:
    struct RPCMessage
    {
        uint32_t msg_id;
        uint32_t req_id;
        uint32_t msg_size;
        std::string msg_body;
    };

    ProtoBuf();
    virtual ~ProtoBuf();

    /* CHAT */
    bool getRequestChatLobbiesMsg(rsctrl::chat::RequestChatLobbies::LobbySet lobbySet, ProtoBuf::RPCMessage& out);
    bool getRequestJoinOrLeaveLobbyMsg(rsctrl::chat::RequestJoinOrLeaveLobby::LobbyAction action, std::string lobbyID, ProtoBuf::RPCMessage& msg);
    bool getRequestRegisterEventsMsg(rsctrl::chat::RequestRegisterEvents::RegisterAction action, ProtoBuf::RPCMessage& msg);
    bool getRequestCreateLobbyMsg(std::string name, std::string topic, rsctrl::chat::LobbyPrivacyLevel provacy, ProtoBuf::RPCMessage& msg);
    bool getRequestSetLobbyNicknameMsg(std::vector<std::string> ids, std::string nick, ProtoBuf::RPCMessage& msg);
    bool getRequestSendMessageMsg(rsctrl::chat::ChatMessage& chatMsg, std::string& answerText, std::string& nick, ProtoBuf::RPCMessage& msg);
    bool getRequestSendMessageMsg(const rsctrl::chat::ChatId& idIn, std::string& answerText, std::string& nick, ProtoBuf::RPCMessage& msg);
    //bool createAnswerToMessage(rsctrl::chat::ChatMessage& chatmsg, std::string& answerText, std::string& nick, ProtoBuf::RPCMessage& msg);

#ifdef ENABLE_DOWNLOAD
    /* FILE */
    bool getRequestStartDownload(std::string& fileName, std::string& fileHash, ProtoBuf::RPCMessage& msg, uint64_t fileSize = 0);
#endif // ENABLE_DOWNLOAD

    static uint8_t getRpcMsgIdSubMsg(uint32_t msg_id);
    static uint16_t getRpcMsgIdService(uint32_t msg_id);
    static uint8_t getRpcMsgIdExtension(uint32_t msg_id);
    static bool isRpcMsgIdResponse(uint32_t msg_id);
    static uint32_t constructMsgId(uint8_t ext, uint16_t service, uint8_t submsg, bool is_response);
protected:
private:
    uint32_t _reqID;

    uint32_t getRequestID();
};

class MsgPacker
{
public:
    static int headersize();
    static bool serialiseHeader(uint32_t msg_id, uint32_t req_id, uint32_t msg_size, uint8_t *buffer, uint32_t bufsize);
    static bool deserialiseHeader(uint32_t &msg_id, uint32_t &req_id, uint32_t &msg_size, uint8_t *buffer, uint32_t bufsize);
};

#endif // PROTOBUF_H
