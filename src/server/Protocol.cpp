/**
 * @file Protocol.cpp
 * @brief Network protocol implementation
 * @author piotrek-pl
 * @date 2025-01-20 13:43:49
 */

#include "Protocol.h"

namespace Protocol {
namespace MessageStructure {

QJsonObject createLoginRequest(const QString& username, const QString& password) {
    return QJsonObject{
        {"type", MessageType::LOGIN},
        {"username", username},
        {"password", password},
        {"protocol_version", PROTOCOL_VERSION}
    };
}

QJsonObject createNewMessage(const QString& content, int from, qint64 timestamp) {
    QJsonObject message;
    message["type"] = MessageType::NEW_MESSAGES;
    message["content"] = content;
    message["from"] = from;
    message["timestamp"] = timestamp;
    return message;
}

QJsonObject createMessageRead(int friendId)
{
    QJsonObject message;
    message["type"] = Protocol::MessageType::MESSAGE_READ;
    message["friendId"] = friendId;
    message["timestamp"] = QDateTime::currentMSecsSinceEpoch();
    return message;
}

QJsonObject createRegisterRequest(const QString& username, const QString& password, const QString& email) {
    return QJsonObject{
        {"type", MessageType::REGISTER},
        {"username", username},
        {"password", password},
        {"email", email},
        {"protocol_version", PROTOCOL_VERSION}
    };
}

QJsonObject createLogoutRequest() {
    return QJsonObject{
        {"type", MessageType::LOGOUT},
        {"timestamp", QDateTime::currentMSecsSinceEpoch()}
    };
}

QJsonObject createMessage(int receiverId, const QString& content) {
    return QJsonObject{
        {"type", MessageType::SEND_MESSAGE},
        {"receiver_id", receiverId},
        {"content", content},
        {"timestamp", QDateTime::currentMSecsSinceEpoch()}
    };
}

QJsonObject createPing() {
    return QJsonObject{
        {"type", MessageType::PING},
        {"timestamp", QDateTime::currentMSecsSinceEpoch()}
    };
}

QJsonObject createPong(qint64 timestamp) {
    return QJsonObject{
        {"type", MessageType::PONG},
        {"timestamp", timestamp}
    };
}

QJsonObject createError(const QString& message) {
    return QJsonObject{
        {"type", MessageType::ERROR},
        {"message", message},
        {"timestamp", QDateTime::currentMSecsSinceEpoch()}
    };
}

QJsonObject createMessageAck(const QString& messageId) {
    return QJsonObject{
        {"type", MessageType::MESSAGE_ACK},
        {"message_id", messageId},
        {"timestamp", QDateTime::currentMSecsSinceEpoch()}
    };
}

QJsonObject createStatusUpdate(const QString& status) {
    return QJsonObject{
        {"type", MessageType::STATUS_UPDATE},
        {"status", status},
        {"timestamp", QDateTime::currentMSecsSinceEpoch()}
    };
}

QJsonObject createGetFriendsList() {
    return QJsonObject{
        {"type", MessageType::GET_FRIENDS_LIST},
        {"timestamp", QDateTime::currentMSecsSinceEpoch()}
    };
}

QJsonObject createFriendsStatusUpdate(const QJsonArray& friends) {
    return QJsonObject{
        {"type", MessageType::FRIENDS_STATUS_UPDATE},
        {"friends", friends},
        {"timestamp", QDateTime::currentMSecsSinceEpoch()}
    };
}

} // namespace MessageStructure
} // namespace Protocol
