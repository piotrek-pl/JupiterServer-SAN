/**
 * @file Protocol.h
 * @brief Network protocol definition
 * @author piotrek-pl
 * @date 2025-01-24 14:34:23
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
const QString GET_CHAT_HISTORY = "get_chat_history";
const QString CHAT_HISTORY_RESPONSE = "chat_history_response";
const QString GET_MORE_HISTORY = "get_more_history";
const QString MORE_HISTORY_RESPONSE = "more_history_response";
const QString GET_LATEST_MESSAGES = "get_latest_messages";
const QString LATEST_MESSAGES_RESPONSE = "latest_messages_response";
const QString NEW_MESSAGES = "new_messages";
const QString MESSAGE_READ = "message_read";
const QString UNREAD_FROM = "unread_from";
const QString MESSAGE_READ_RESPONSE = "message_read_response";
const QString SEARCH_USERS = "search_users";
const QString SEARCH_USERS_RESPONSE = "search_users_response";
const QString REMOVE_FRIEND = "remove_friend";
const QString REMOVE_FRIEND_RESPONSE = "remove_friend_response";
const QString FRIEND_REMOVED = "friend_removed";

// Friend Request Messages
const QString ADD_FRIEND_REQUEST = "add_friend_request";
const QString ADD_FRIEND_RESPONSE = "add_friend_response";
const QString FRIEND_REQUEST_RECEIVED = "friend_request_received";
const QString FRIEND_REQUEST_ACCEPT = "friend_request_accept";
const QString FRIEND_REQUEST_REJECT = "friend_request_reject";
const QString FRIEND_REQUEST_ACCEPT_RESPONSE = "friend_request_accept_response";
const QString FRIEND_REQUEST_REJECT_RESPONSE = "friend_request_reject_response";
const QString GET_SENT_INVITATIONS = "get_sent_invitations";
const QString GET_RECEIVED_INVITATIONS = "get_received_invitations";
const QString SENT_INVITATIONS_RESPONSE = "sent_invitations_response";
const QString RECEIVED_INVITATIONS_RESPONSE = "received_invitations_response";
const QString CANCEL_FRIEND_REQUEST = "cancel_friend_request";
const QString CANCEL_FRIEND_REQUEST_RESPONSE = "cancel_friend_request_response";

// Invitation System Messages
const QString SEND_INVITATION = "send_invitation";
const QString INVITATION_RESPONSE = "invitation_response";
const QString INVITATION_ACCEPTED = "invitation_accepted";
const QString INVITATION_REJECTED = "invitation_rejected";
const QString INVITATION_CANCELLED = "invitation_cancelled";
const QString GET_INVITATIONS = "get_invitations";
const QString INVITATIONS_LIST = "invitations_list";
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
    MessageType::MESSAGE_ACK,
    MessageType::GET_CHAT_HISTORY,
    MessageType::GET_MORE_HISTORY,
    MessageType::NEW_MESSAGES,
    MessageType::REMOVE_FRIEND,
    MessageType::REMOVE_FRIEND_RESPONSE,
    MessageType::SEARCH_USERS,
    MessageType::SEARCH_USERS_RESPONSE,
    // Friend Request System
    MessageType::ADD_FRIEND_REQUEST,
    MessageType::ADD_FRIEND_RESPONSE,
    MessageType::FRIEND_REQUEST_RECEIVED,
    MessageType::FRIEND_REQUEST_ACCEPT,
    MessageType::FRIEND_REQUEST_REJECT,
    MessageType::FRIEND_REQUEST_ACCEPT_RESPONSE,
    MessageType::FRIEND_REQUEST_REJECT_RESPONSE,
    MessageType::GET_SENT_INVITATIONS,
    MessageType::GET_RECEIVED_INVITATIONS,
    MessageType::CANCEL_FRIEND_REQUEST,
    MessageType::CANCEL_FRIEND_REQUEST_RESPONSE,
    // Invitation System
    MessageType::SEND_INVITATION,
    MessageType::INVITATION_RESPONSE,
    MessageType::INVITATION_ACCEPTED,
    MessageType::INVITATION_REJECTED,
    MessageType::INVITATION_CANCELLED,
    MessageType::GET_INVITATIONS,
    MessageType::INVITATIONS_LIST
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
QJsonObject createMessageRead(int friendId);
QJsonObject createMessageReadResponse();

// Ping/Pong
QJsonObject createPing();
QJsonObject createPong(qint64 timestamp);

// Błędy
QJsonObject createError(const QString& message);

// Lista znajomych
QJsonObject createGetFriendsList();
QJsonObject createFriendsStatusUpdate(const QJsonArray& friends);
QJsonObject createRemoveFriendRequest(int friendId);
QJsonObject createRemoveFriendResponse(bool success);
QJsonObject createFriendRemovedNotification(int friendId);

// Wiadomości czatu
QJsonObject createNewMessage(const QString& content, int from, qint64 timestamp);

// Wyszukiwanie użytkowników
QJsonObject createSearchUsersRequest(const QString& query);
QJsonObject createSearchUsersResponse(const QJsonArray& users);

// Friend Request System
QJsonObject createAddFriendRequest(int userId);
QJsonObject createAddFriendResponse(bool success, const QString& message = "");
QJsonObject createFriendRequestReceivedNotification(int fromUserId, const QString& username);
QJsonObject createFriendRequestAccept(int requestId);
QJsonObject createFriendRequestReject(int requestId);
QJsonObject createFriendRequestAcceptResponse(bool success, const QString& message = "");
QJsonObject createFriendRequestRejectResponse(bool success, const QString& message = "");
QJsonObject createGetSentInvitationsRequest();
QJsonObject createGetReceivedInvitationsRequest();
QJsonObject createSentInvitationsResponse(const QJsonArray& invitations);
QJsonObject createReceivedInvitationsResponse(const QJsonArray& invitations);
QJsonObject createCancelFriendRequest(int requestId);
QJsonObject createCancelFriendRequestResponse(bool success, const QString& message = "");

// Invitation System
static QJsonObject createInvitationResponse(bool success, const QString& message = "");
static QJsonObject createInvitationsList(const QJsonArray& invitations, bool sent = true);

} // namespace MessageStructure

namespace ChatHistory {
const int MESSAGE_BATCH_SIZE = 20;  // ilość wiadomości w jednej paczce
}

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

// Status zaproszenia
namespace InvitationStatus {
const QString PENDING = "pending";
const QString ACCEPTED = "accepted";
const QString REJECTED = "rejected";
const QString CANCELLED = "cancelled";
}

} // namespace Protocol

#endif // PROTOCOL_H
