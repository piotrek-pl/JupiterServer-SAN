/**
 * @file Protocol.cpp
 * @brief Network protocol implementation
 * @author piotrek-pl
 * @date 2025-01-27 01:02:05
 */

#include "Protocol.h"

namespace Protocol {
namespace MessageStructure {

// Basic operations
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

QJsonObject createMessageRead(int friendId) {
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

// Ping/Pong operations
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

// Status operations
QJsonObject createStatusUpdate(const QString& status) {
    return QJsonObject{
        {"type", MessageType::STATUS_UPDATE},
        {"status", status},
        {"timestamp", QDateTime::currentMSecsSinceEpoch()}
    };
}

// Friends list operations
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

QJsonObject createMessageReadResponse() {
    return QJsonObject{
        {"type", MessageType::MESSAGE_READ_RESPONSE},
        {"status", "success"},
        {"timestamp", QDateTime::currentMSecsSinceEpoch()}
    };
}

// Search operations
QJsonObject createSearchUsersRequest(const QString& query) {
    return QJsonObject{
        {"type", MessageType::SEARCH_USERS},
        {"query", query},
        {"timestamp", QDateTime::currentMSecsSinceEpoch()}
    };
}

QJsonObject createSearchUsersResponse(const QJsonArray& users) {
    return QJsonObject{
        {"type", MessageType::SEARCH_USERS_RESPONSE},
        {"users", users},
        {"timestamp", QDateTime::currentMSecsSinceEpoch()}
    };
}

// Friend management operations
QJsonObject createRemoveFriendRequest(int friendId) {
    return QJsonObject{
        {"type", MessageType::REMOVE_FRIEND},
        {"friend_id", friendId},
        {"timestamp", QDateTime::currentMSecsSinceEpoch()}
    };
}

QJsonObject createRemoveFriendResponse(bool success) {
    return QJsonObject{
        {"type", MessageType::REMOVE_FRIEND_RESPONSE},
        {"status", success ? "success" : "error"},
        {"timestamp", QDateTime::currentMSecsSinceEpoch()}
    };
}

QJsonObject createFriendRemovedNotification(int friendId) {
    return QJsonObject{
        {"type", MessageType::FRIEND_REMOVED},
        {"friend_id", friendId},
        {"timestamp", QDateTime::currentMSecsSinceEpoch()}
    };
}

// Friend Request System
QJsonObject createAddFriendRequest(int userId) {
    return QJsonObject{
        {"type", MessageType::ADD_FRIEND_REQUEST},
        {"user_id", userId},
        {"timestamp", QDateTime::currentMSecsSinceEpoch()}
    };
}

QJsonObject createAddFriendResponse(bool success, const QString& message) {
    return QJsonObject{
        {"type", MessageType::ADD_FRIEND_RESPONSE},
        {"status", success ? "success" : "error"},
        {"message", message},
        {"timestamp", QDateTime::currentMSecsSinceEpoch()}
    };
}

QJsonObject createFriendRequestReceivedNotification(int fromUserId, const QString& username) {
    return QJsonObject{
        {"type", MessageType::FRIEND_REQUEST_RECEIVED},
        {"from_user_id", fromUserId},
        {"username", username},
        {"timestamp", QDateTime::currentMSecsSinceEpoch()}
    };
}

QJsonObject createFriendRequestAccept(int requestId) {
    return QJsonObject{
        {"type", MessageType::FRIEND_REQUEST_ACCEPT},
        {"request_id", requestId},
        {"timestamp", QDateTime::currentMSecsSinceEpoch()}
    };
}

QJsonObject createFriendRequestReject(int requestId) {
    return QJsonObject{
        {"type", MessageType::FRIEND_REQUEST_REJECT},
        {"request_id", requestId},
        {"timestamp", QDateTime::currentMSecsSinceEpoch()}
    };
}

QJsonObject createFriendRequestAcceptResponse(bool success, const QString& message) {
    return QJsonObject{
        {"type", MessageType::FRIEND_REQUEST_ACCEPT_RESPONSE},
        {"status", success ? "success" : "error"},
        {"message", message},
        {"timestamp", QDateTime::currentMSecsSinceEpoch()}
    };
}

QJsonObject createFriendRequestRejectResponse(bool success, const QString& message) {
    return QJsonObject{
        {"type", MessageType::FRIEND_REQUEST_REJECT_RESPONSE},
        {"status", success ? "success" : "error"},
        {"message", message},
        {"timestamp", QDateTime::currentMSecsSinceEpoch()}
    };
}

QJsonObject createGetSentInvitationsRequest() {
    return QJsonObject{
        {"type", MessageType::GET_SENT_INVITATIONS},
        {"timestamp", QDateTime::currentMSecsSinceEpoch()}
    };
}

QJsonObject createGetReceivedInvitationsRequest() {
    return QJsonObject{
        {"type", MessageType::GET_RECEIVED_INVITATIONS},
        {"timestamp", QDateTime::currentMSecsSinceEpoch()}
    };
}

QJsonObject createSentInvitationsResponse(const QJsonArray& invitations) {
    return QJsonObject{
        {"type", MessageType::SENT_INVITATIONS_RESPONSE},
        {"invitations", invitations},
        {"timestamp", QDateTime::currentMSecsSinceEpoch()}
    };
}

QJsonObject createReceivedInvitationsResponse(const QJsonArray& invitations) {
    return QJsonObject{
        {"type", MessageType::RECEIVED_INVITATIONS_RESPONSE},
        {"invitations", invitations},
        {"timestamp", QDateTime::currentMSecsSinceEpoch()}
    };
}

QJsonObject createCancelFriendRequest(int requestId) {
    return QJsonObject{
        {"type", MessageType::CANCEL_FRIEND_REQUEST},
        {"request_id", requestId},
        {"timestamp", QDateTime::currentMSecsSinceEpoch()}
    };
}

QJsonObject createCancelFriendRequestResponse(bool success, const QString& message) {
    return QJsonObject{
        {"type", MessageType::CANCEL_FRIEND_REQUEST_RESPONSE},
        {"status", success ? "success" : "error"},
        {"message", message},
        {"timestamp", QDateTime::currentMSecsSinceEpoch()}
    };
}

// Invitation System
QJsonObject createInvitationResponse(bool success, const QString& message) {
    QJsonObject response;
    response["type"] = MessageType::INVITATION_RESPONSE;
    response["success"] = success;
    if (!message.isEmpty()) {
        response["message"] = message;
    }
    response["timestamp"] = QDateTime::currentMSecsSinceEpoch();
    return response;
}

QJsonObject createInvitationsList(const QJsonArray& invitations, bool sent) {
    QJsonObject response;
    response["type"] = MessageType::INVITATIONS_LIST;
    response["invitations"] = invitations;
    response["sent"] = sent;
    response["timestamp"] = QDateTime::currentMSecsSinceEpoch();
    return response;
}

QJsonObject createInvitationAlreadyExistsResponse(int userId, const QString& username) {
    return QJsonObject{
        {"type", MessageType::INVITATION_ALREADY_EXISTS},
        {"user_id", userId},
        {"username", username},
        {"status", "error"},
        {"error_code", "INVITATION_ALREADY_EXISTS"},
        {"message", "Invitation already sent to this user"},
        {"timestamp", QDateTime::currentMSecsSinceEpoch()}
    };
}

QJsonObject createInvitationStatusChangedNotification(int requestId, int userId, const QString& status) {
    return QJsonObject{
        {"type", MessageType::INVITATION_STATUS_CHANGED},
        {"request_id", requestId},
        {"user_id", userId},
        {"status", status},
        {"timestamp", QDateTime::currentMSecsSinceEpoch()}
    };
}

QJsonObject createFriendRequestAcceptedNotification(int userId, const QString& username) {
    return QJsonObject{
        {"type", MessageType::FRIEND_REQUEST_ACCEPTED_NOTIFICATION},
        {"user_id", userId},
        {"username", username},
        {"timestamp", QDateTime::currentMSecsSinceEpoch()}
    };
}

QJsonObject createFriendRequestCancelledNotification(int requestId, int fromUserId) {
    return QJsonObject{
        {"type", MessageType::FRIEND_REQUEST_CANCELLED_NOTIFICATION},
        {"request_id", requestId},
        {"from_user_id", fromUserId},
        {"timestamp", QDateTime::currentMSecsSinceEpoch()}
    };
}

} // namespace MessageStructure
} // namespace Protocol
