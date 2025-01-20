/**
 * @file Protocol.h
 * @brief Network protocol definition
 * @author piotrek-pl
 * @date 2025-01-20 15:14:09
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QStringList>

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
const QString LOGOUT_RESPONSE = "logout_response";
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

// Status użytkownika
namespace UserStatus {
const QString ONLINE = "online";
const QString OFFLINE = "offline";
const QString AWAY = "away";
const QString BUSY = "busy";
}

// Stan sesji
namespace SessionState {
const QString INITIAL = "initial";         // Stan początkowy po połączeniu
const QString AUTHENTICATING = "authenticating"; // W trakcie procesu logowania
const QString AUTHENTICATED = "authenticated";   // Zalogowany
const QString DISCONNECTING = "disconnecting";  // W trakcie rozłączania
}

// Dozwolone wiadomości dla każdego stanu
namespace AllowedMessages {
const QStringList INITIAL = {
    MessageType::PING,
    MessageType::PONG,
    MessageType::LOGIN,
    MessageType::REGISTER
};

const QStringList AUTHENTICATING = {
    MessageType::PING,
    MessageType::PONG,
    MessageType::LOGIN,
    MessageType::LOGIN_RESPONSE
};

const QStringList AUTHENTICATED = {
    MessageType::PING,
    MessageType::PONG,
    MessageType::LOGOUT,
    MessageType::GET_STATUS,
    MessageType::GET_FRIENDS_LIST,
    MessageType::GET_MESSAGES,
    MessageType::SEND_MESSAGE,
    MessageType::MESSAGE_ACK
};

const QStringList DISCONNECTING = {
    MessageType::PING,
    MessageType::PONG,
    MessageType::LOGOUT_RESPONSE
};
}

// Struktury wiadomości
namespace MessageStructure {
// Podstawowe operacje
QJsonObject createLoginRequest(const QString& username, const QString& password);
QJsonObject createRegisterRequest(const QString& username, const QString& password, const QString& email);
QJsonObject createLogoutRequest();

// Wiadomości i statusy
QJsonObject createMessage(int receiverId, const QString& content);
QJsonObject createMessageAck(const QString& messageId);
QJsonObject createStatusUpdate(const QString& status);

// Ping/Pong
QJsonObject createPing();
QJsonObject createPong(qint64 timestamp);

// Błędy
QJsonObject createError(const QString& message);

// Lista znajomych
QJsonObject createGetFriendsList();
QJsonObject createFriendsStatusUpdate(const QJsonArray& friends);

} // namespace MessageStructure

// Walidacja wiadomości
namespace MessageValidation {
inline bool isMessageAllowedInState(const QString& messageType, const QString& state) {
    if (messageType == MessageType::PING || messageType == MessageType::PONG) {
        return true;  // Zawsze dozwolone
    }

    if (state == SessionState::INITIAL) {
        return AllowedMessages::INITIAL.contains(messageType);
    }
    else if (state == SessionState::AUTHENTICATING) {
        return AllowedMessages::AUTHENTICATING.contains(messageType);
    }
    else if (state == SessionState::AUTHENTICATED) {
        return AllowedMessages::AUTHENTICATED.contains(messageType);
    }
    else if (state == SessionState::DISCONNECTING) {
        return AllowedMessages::DISCONNECTING.contains(messageType);
    }

    return false;
}
} // namespace MessageValidation

} // namespace Protocol

#endif // PROTOCOL_H
