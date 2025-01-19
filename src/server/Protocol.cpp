#include "Protocol.h"

namespace Protocol {
namespace MessageStructure {

QJsonObject createLoginRequest(const QString& username, const QString& password)
{
    return QJsonObject{
        {"type", MessageType::LOGIN},
        {"username", username},
        {"password", password},
        {"protocol_version", PROTOCOL_VERSION}
    };
}

QJsonObject createMessage(int receiverId, const QString& content)
{
    return QJsonObject{
        {"type", MessageType::SEND_MESSAGE},
        {"receiver_id", receiverId},
        {"content", content},
        {"timestamp", QDateTime::currentMSecsSinceEpoch()}
    };
}

QJsonObject createPing()
{
    QJsonObject msg;
    msg["type"] = MessageType::PING;
    msg["timestamp"] = QDateTime::currentMSecsSinceEpoch();
    return msg;
}

QJsonObject createPong(qint64 timestamp)
{
    QJsonObject msg;
    msg["type"] = MessageType::PONG;
    msg["timestamp"] = timestamp;
    return msg;
}

QJsonObject createError(const QString& message)
{
    return QJsonObject{
        {"type", MessageType::ERROR},
        {"message", message},
        {"timestamp", QDateTime::currentMSecsSinceEpoch()}
    };
}

QJsonObject createMessageAck(const QString& messageId)
{
    return QJsonObject{
        {"type", MessageType::MESSAGE_ACK},
        {"message_id", messageId},
        {"timestamp", QDateTime::currentMSecsSinceEpoch()}
    };
}

QJsonObject createStatusUpdate(const QString& status)
{
    return QJsonObject{
        {"type", MessageType::STATUS_UPDATE},
        {"status", status},
        {"timestamp", QDateTime::currentMSecsSinceEpoch()}
    };
}

QJsonObject createFriendsStatusUpdate(const QJsonArray& friends)
{
    return QJsonObject{
        {"type", MessageType::FRIENDS_STATUS_UPDATE},
        {"friends", friends},
        {"timestamp", QDateTime::currentMSecsSinceEpoch()}
    };
}

} // namespace MessageStructure
} // namespace Protocol
