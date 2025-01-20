#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <QString>
#include <QJsonObject>
#include <QDateTime>

namespace Protocol {

// Wersja protokołu
constexpr int PROTOCOL_VERSION = 1;

// Timeouty (w milisekundach)
namespace Timeouts {
constexpr int CONNECTION = 30000; // 30 sekund
constexpr int REQUEST = 15000; // 15 sekund
constexpr int PING = 10000; // 10 sekund
constexpr int RECONNECT = 5000; // 5 sekund
constexpr int STATUS_UPDATE = 15000; // 15 sekund
}

// Typy wiadomości
namespace MessageType {
const QString LOGIN = "login";
const QString LOGIN_RESPONSE = "login_response";
const QString REGISTER = "register";
const QString REGISTER_RESPONSE = "register_response";
const QString LOGOUT = "logout";
const QString LOGOUT_RESPONSE = "logout_response";  // Dodaj LOGOUT_RESPONSE
const QString GET_STATUS = "get_status";
const QString STATUS_UPDATE = "status_response";
const QString GET_FRIENDS_LIST = "get_friends_list";
const QString FRIENDS_LIST_RESPONSE = "friends_list_response";
const QString FRIENDS_STATUS_UPDATE = "friends_status_update";
const QString SEND_MESSAGE = "send_message";
const QString MESSAGE_RESPONSE = "message_response";
const QString MESSAGE_ACK = "message_ack";
const QString GET_MESSAGES = "get_messages";
const QString PENDING_MESSAGES = "pending_messages";
const QString ERROR = "error";
const QString PING = "ping";
const QString PONG = "pong";
}

// Struktury wiadomości
namespace MessageStructure {
QJsonObject createLoginRequest(const QString& username, const QString& password);
QJsonObject createMessage(int receiverId, const QString& content);
QJsonObject createPing();
QJsonObject createPong(qint64 timestamp);
QJsonObject createError(const QString& message);
QJsonObject createMessageAck(const QString& messageId);
QJsonObject createStatusUpdate(const QString& status);
QJsonObject createFriendsStatusUpdate(const QJsonArray& friends);
}

} // namespace Protocol

#endif // PROTOCOL_H
