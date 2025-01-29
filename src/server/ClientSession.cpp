/**
 * @file ClientSession.cpp
 * @brief Implementation of the ClientSession class
 * @author piotrek-pl
 * @date 2025-01-22 09:34:34
 */

#include "ClientSession.h"
#include "database/DatabaseManager.h"
#include "network/Protocol.h"
#include "ActiveSessions.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUuid>
#include <QDateTime>
#include <QThread>

ClientSession::ClientSession(QTcpSocket* socket, DatabaseManager* dbManager, QObject *parent)
    : QObject(parent)
    , socket(socket)
    , dbManager(dbManager)
    , userId(0)
    , isAuthenticated(false)
    , state(Protocol::SessionState::INITIAL)
    , lastPingTime(QDateTime::currentMSecsSinceEpoch())
    , missedPings(0)
    , lastSentMessageId(0)
{
    qDebug() << "ClientSession constructor called";

    sessionConnectionName = QString("Session_%1").arg(
        reinterpret_cast<quintptr>(this),
        0, 16);

    qDebug() << "Creating new database connection:" << sessionConnectionName;

    if (!dbManager->cloneConnectionForThread(sessionConnectionName)) {
        qWarning() << "Failed to create database connection for session:"
                   << sessionConnectionName;
    } else {
        qDebug() << "Successfully created database connection for session:"
                 << sessionConnectionName;
    }

    connect(socket, &QTcpSocket::readyRead,
            this, &ClientSession::handleReadyRead);
    connect(socket, &QTcpSocket::errorOccurred,
            this, &ClientSession::handleError);

    statusUpdateTimer.setInterval(Protocol::Timeouts::STATUS_UPDATE);
    connect(&statusUpdateTimer, &QTimer::timeout,
            this, &ClientSession::sendFriendsStatusUpdate);

    pingTimer.setInterval(Protocol::Timeouts::PING);
    connect(&pingTimer, &QTimer::timeout,
            this, &ClientSession::checkConnectionStatus);
    pingTimer.start();

    qDebug() << "New client session created";
}

ClientSession::~ClientSession()
{
    if (userId > 0) {
        ActiveSessions::getInstance().removeSession(userId);
    }

    qDebug() << "ClientSession destructor called";

    if (QSqlDatabase::contains(sessionConnectionName)) {
        qDebug() << "Closing database connection:" << sessionConnectionName;
        {
            QSqlDatabase db = QSqlDatabase::database(sessionConnectionName);
            if (db.isOpen()) {
                db.close();
            }
        }
        QSqlDatabase::removeDatabase(sessionConnectionName);
    }

    statusUpdateTimer.stop();
    messagesCheckTimer.stop();
    pingTimer.stop();

    if (socket) {
        socket->disconnectFromHost();
        socket->deleteLater();
        socket = nullptr;
    }

    qDebug() << "Client session destroyed";
}

void ClientSession::handleReadyRead()
{
    qDebug() << "SERVER: handleReadyRead called, bytes available:"
             << socket->bytesAvailable();

    if (!socket->bytesAvailable()) {
        qDebug() << "SERVER: No bytes available";
        return;
    }

    QByteArray newData = socket->readAll();
    qDebug() << "SERVER: Read" << newData.size() << "bytes:"
             << QString::fromUtf8(newData);

    buffer.append(newData);
    qDebug() << "SERVER: Buffer size after append:" << buffer.size();

    processBuffer();
}

void ClientSession::handleError(QAbstractSocket::SocketError socketError)
{
    qWarning() << "Socket error:" << socketError
               << "Error string:" << socket->errorString();

    switch (socketError) {
    case QAbstractSocket::RemoteHostClosedError:
        qDebug() << "Remote host closed connection";
        break;
    case QAbstractSocket::HostNotFoundError:
        qDebug() << "Host not found";
        break;
    case QAbstractSocket::ConnectionRefusedError:
        qDebug() << "Connection refused";
        break;
    default:
        qDebug() << "Unknown socket error occurred";
    }

    if (isAuthenticated && userId > 0) {
        dbManager->updateUserStatus(userId, "offline");
    }
}

void ClientSession::processMessage(const QByteArray& message)
{
    qDebug() << "SERVER: Received message of size:" << message.size();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(message, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "SERVER: Failed to parse message:" << parseError.errorString();
        qWarning() << "SERVER: Raw message:" << message;
        sendResponse(QJsonDocument(Protocol::MessageStructure::createError("Invalid JSON format")).toJson());
        return;
    }

    QJsonObject json = doc.object();
    QString type = json["type"].toString();

    qDebug() << "SERVER: Received message type:" << type;

    if (type == Protocol::MessageType::PING) {
        qDebug() << "SERVER: Processing PING message";
        handlePing(json);
        return;
    }
    else if (type == Protocol::MessageType::PONG) {
        lastPingTime = QDateTime::currentMSecsSinceEpoch();
        missedPings = 0;
        return;
    }

    if (!isAuthenticated && !Protocol::AllowedMessages::INITIAL.contains(type)) {
        sendResponse(QJsonDocument(Protocol::MessageStructure::createError("Not authenticated")).toJson());
        return;
    }

    if (type == Protocol::MessageType::LOGIN) {
        handleLogin(json);
    }
    else if (type == Protocol::MessageType::REGISTER) {
        handleRegister(json);
    }
    else if (type == Protocol::MessageType::GET_FRIENDS_LIST) {
        handleFriendsListRequest();
    }
    else if (type == Protocol::MessageType::GET_STATUS) {
        handleStatusRequest();
    }
    else if (type == Protocol::MessageType::STATUS_UPDATE) {
        handleStatusUpdate(json);
    }
    else if (type == Protocol::MessageType::SEARCH_USERS) {
        handleSearchUsers(json);
    }
    else if (type == Protocol::MessageType::REMOVE_FRIEND) {
        handleRemoveFriend(json);
    }
    else if (type == Protocol::MessageType::GET_LATEST_MESSAGES) {
        handleGetLatestMessages(json);
    }
    else if (type == Protocol::MessageType::GET_CHAT_HISTORY) {
        handleGetChatHistory(json);
    }
    else if (type == Protocol::MessageType::GET_MORE_HISTORY) {
        handleGetMoreHistory(json);
    }
    else if (type == Protocol::MessageType::SEND_MESSAGE) {
        handleSendMessage(json);
    }
    else if (type == Protocol::MessageType::MESSAGE_READ) {
        handleMessageRead(json);
    }
    else if (type == Protocol::MessageType::ADD_FRIEND_REQUEST) {
        handleAddFriendRequest(json);
    }
    else if (type == Protocol::MessageType::GET_RECEIVED_INVITATIONS) {
        handleGetReceivedInvitations();
    }
    else if (type == Protocol::MessageType::GET_SENT_INVITATIONS) {
        handleGetSentInvitations();
    }
    else if (type == Protocol::MessageType::CANCEL_FRIEND_REQUEST) {
        handleCancelFriendRequest(json);
    }
    else if (type == Protocol::MessageType::FRIEND_REQUEST_ACCEPT) {
        handleFriendRequestAccept(json);
    }
    else if (type == Protocol::MessageType::FRIEND_REQUEST_REJECT) {
        handleFriendRequestReject(json);
    }
    else if (type == Protocol::MessageType::LOGOUT) {
        handleLogout();
    }
    else {
        qWarning() << "Unknown message type:" << type;
        sendResponse(QJsonDocument(Protocol::MessageStructure::createError("Unknown message type")).toJson());
    }
}

void ClientSession::handleLogin(const QJsonObject& json)
{
    QString username = json["username"].toString();
    QString password = json["password"].toString();

    qDebug() << "SERVER: Processing login request for user:" << username;

    if (username.isEmpty() || password.isEmpty()) {
        state = Protocol::SessionState::INITIAL;
        QJsonObject errorResponse = Protocol::MessageStructure::createError("Invalid credentials");
        sendResponse(QJsonDocument(errorResponse).toJson());
        return;
    }

    QSqlDatabase sessionDb = QSqlDatabase::database(sessionConnectionName);
    if (!sessionDb.isOpen()) {
        qWarning() << "Session database connection is not open! Attempting to reopen...";
        if (!dbManager->cloneConnectionForThread(sessionConnectionName)) {
            QJsonObject errorResponse = Protocol::MessageStructure::createError("Database connection error");
            sendResponse(QJsonDocument(errorResponse).toJson());
            return;
        }
    }

    if (dbManager->authenticateUser(username, password, userId)) {
        setUserId(userId);
        state = Protocol::SessionState::AUTHENTICATED;
        isAuthenticated = true;

        // Najpierw wyślij odpowiedź o udanym logowaniu
        QJsonObject response{
            {"type", Protocol::MessageType::LOGIN_RESPONSE},
            {"status", "success"},
            {"userId", static_cast<int>(userId)},
            {"timestamp", QDateTime::currentMSecsSinceEpoch()}
        };

        qDebug() << "SERVER: Sending login success response for user:" << username;
        sendResponse(QJsonDocument(response).toJson());

        // Następnie aktualizuj status i wykonaj pozostałe operacje
        dbManager->updateUserStatus(userId, "online");
        statusUpdateTimer.start();
        sendUnreadFromUsers();
        handleFriendsListRequest();

        qDebug() << "SERVER: User" << username << "logged in successfully";
    } else {
        state = Protocol::SessionState::INITIAL;
        QJsonObject errorResponse = Protocol::MessageStructure::createError("Authentication failed");
        sendResponse(QJsonDocument(errorResponse).toJson());
        qDebug() << "SERVER: Failed login attempt for user:" << username;
    }
}

void ClientSession::handleRegister(const QJsonObject& json)
{
    QString username = json["username"].toString();
    QString password = json["password"].toString();
    QString email = json["email"].toString();

    if (username.isEmpty() || password.isEmpty() || email.isEmpty()) {
        sendResponse(QJsonDocument(Protocol::MessageStructure::createError("Invalid registration data")).toJson());
        return;
    }

    if (password.length() < Protocol::Validation::MIN_PASSWORD_LENGTH) {
        sendResponse(QJsonDocument(Protocol::MessageStructure::createError(
                                       QString("Password must be at least %1 characters long").arg(Protocol::Validation::MIN_PASSWORD_LENGTH))).toJson());
        return;
    }


    if (dbManager->registerUser(username, password, email)) {
        QJsonObject response{
            {"type", Protocol::MessageType::REGISTER_RESPONSE},
            {"status", "success"},
            {"message", "Registration successful"},
            {"timestamp", QDateTime::currentMSecsSinceEpoch()}
        };
        sendResponse(QJsonDocument(response).toJson());
        qDebug() << "New user registered:" << username;
    } else {
        sendResponse(QJsonDocument(Protocol::MessageStructure::createError("Registration failed")).toJson());
        qDebug() << "Failed registration attempt for username:" << username;
    }
}

void ClientSession::handleLogout()
{
    if (isAuthenticated && userId > 0) {
        dbManager->updateUserStatus(userId, "offline");
        isAuthenticated = false;
        userId = 0;

        statusUpdateTimer.stop();

        QJsonObject response{
            {"type", "logout_response"},
            {"status", "success"},
            {"timestamp", QDateTime::currentMSecsSinceEpoch()}
        };
        sendResponse(QJsonDocument(response).toJson());

        qDebug() << "User logged out successfully";
    }
}

void ClientSession::handlePing(const QJsonObject& message)
{
    qDebug() << "SERVER: Received PING from client at"
             << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");

    // Aktualizuj czas ostatniego pinga
    lastPingTime = QDateTime::currentMSecsSinceEpoch();
    missedPings = 0;

    // Utwórz i wyślij odpowiedź PONG
    QJsonObject pongResponse{
        {"type", Protocol::MessageType::PONG},
        {"timestamp", message["timestamp"].toInteger()}, // użyj tego samego timestampa co w PING
    };

    qDebug() << "SERVER: Sending PONG response at"
             << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");

    sendResponse(QJsonDocument(pongResponse).toJson());
}

void ClientSession::handleSendMessage(const QJsonObject& json)
{
    int receiverId = json["receiver_id"].toInt();
    QString content = json["content"].toString();
    QString messageId = QUuid::createUuid().toString();

    if (content.isEmpty()) {
        sendResponse(QJsonDocument(Protocol::MessageStructure::createError("Empty message content")).toJson());
        return;
    }

    // Próba zapisania wiadomości
    if (dbManager->storeMessage(userId, receiverId, content)) {
        // Wyślij potwierdzenie do nadawcy
        QJsonObject response = Protocol::MessageStructure::createMessageAck(messageId);
        sendResponse(QJsonDocument(response).toJson());

        // Wyślij wiadomość do odbiorcy jeśli jest online
        ClientSession* receiverSession = ActiveSessions::getInstance().getSession(receiverId);
        if (receiverSession) {
            // Użyj protokołu do utworzenia wiadomości - używamy createNewMessage zamiast createNewMessageResponse
            QJsonObject newMessage = Protocol::MessageStructure::createNewMessage(
                content,
                static_cast<int>(userId),
                QDateTime::currentMSecsSinceEpoch()
                );
            receiverSession->sendResponse(QJsonDocument(newMessage).toJson());
        }

        qDebug() << "Message" << messageId << "stored and sent successfully";
    } else {
        sendResponse(QJsonDocument(Protocol::MessageStructure::createError("Failed to store message")).toJson());
        qWarning() << "Failed to store message" << messageId;
    }
}

void ClientSession::checkConnectionStatus()
{
    if (!socket || !socket->isValid() || socket->state() != QAbstractSocket::ConnectedState) {
        return;
    }

    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();

    QJsonObject pingMessage = Protocol::MessageStructure::createPing();
    pingMessage["timestamp"] = currentTime;

    qDebug() << "SERVER: Sending PING at"
             << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz")
             << "with timestamp:" << currentTime;

    sendResponse(QJsonDocument(pingMessage).toJson());

    if (currentTime - lastPingTime > Protocol::Timeouts::CONNECTION) {
        missedPings++;
        qWarning() << "Missed PONG from client - count:" << missedPings;

        if (missedPings >= MAX_MISSED_PINGS) {
            qWarning() << "Connection timeout - closing session";
            socket->disconnectFromHost();
        }
    }
}

void ClientSession::handleFriendsListRequest()
{
    QJsonObject response = prepareFriendsListResponse();
    sendResponse(QJsonDocument(response).toJson());
}

void ClientSession::handleStatusRequest()
{
    QJsonObject response = prepareStatusResponse();
    sendResponse(QJsonDocument(response).toJson());
}

QJsonObject ClientSession::prepareFriendsListResponse()
{
    QJsonObject response;
    response["type"] = Protocol::MessageType::FRIENDS_LIST_RESPONSE;

    QJsonArray friendsArray;
    auto friendsList = dbManager->getFriendsList(userId);

    for (const auto& friend_ : friendsList) {
        QJsonObject friendObj;
        int friendId = static_cast<int>(friend_.first);
        friendObj["id"] = friendId;
        friendObj["username"] = friend_.second;

        QString status;
        if (dbManager->getUserStatus(friendId, status)) {
            friendObj["status"] = status;
        } else {
            qWarning() << "Failed to get status for user ID:" << friendId;
            friendObj["status"] = Protocol::UserStatus::OFFLINE;
        }

        qDebug() << "Friend" << friend_.second << "status:" << friendObj["status"].toString();
        friendsArray.append(friendObj);
    }

    response["friends"] = friendsArray;
    response["timestamp"] = QDateTime::currentMSecsSinceEpoch();

    qDebug() << "Prepared friends list response:" << QJsonDocument(response).toJson();

    return response;
}

QJsonObject ClientSession::prepareStatusResponse()
{
    return Protocol::MessageStructure::createStatusUpdate("online");
}

QJsonObject ClientSession::prepareMessagesResponse()
{
    QJsonObject response;
    QJsonArray messagesArray;

    for (const auto& msg : messages) {
        QJsonObject msgObj;
        msgObj["sender"] = msg.username;
        msgObj["content"] = msg.message;
        msgObj["timestamp"] = msg.timestamp.toString(Qt::ISODate);
        msgObj["isRead"] = msg.isRead;
        messagesArray.append(msgObj);
    }

    response["type"] = Protocol::MessageType::CHAT_HISTORY_RESPONSE;
    response["messages"] = messagesArray;
    return response;
}

void ClientSession::sendResponse(const QByteArray& response)
{
    if (!socket || !socket->isValid()) {
        qWarning() << "Attempting to send response through invalid socket";
        return;
    }

    qint64 written = socket->write(response);
    if (written <= 0) {
        qWarning() << "Failed to write to socket";
        return;
    }

    socket->flush();
}

void ClientSession::sendFriendsStatusUpdate()
{
    if (isAuthenticated) {
        auto friendsList = prepareFriendsListResponse()["friends"].toArray();
        QJsonObject response = Protocol::MessageStructure::createFriendsStatusUpdate(friendsList);
        sendResponse(QJsonDocument(response).toJson());
    }
}

void ClientSession::processBuffer()
{
    qDebug() << "SERVER: Processing buffer of size:" << buffer.size()
    << "Content:" << QString::fromUtf8(buffer);

    bool foundJson;
    do {
        foundJson = false;
        int startPos = buffer.indexOf('{');

        if (startPos >= 0) {
            qDebug() << "SERVER: Found JSON start at position:" << startPos;

            int endPos = -1;
            int braceCount = 1;

            for (int i = startPos + 1; i < buffer.size(); ++i) {
                if (buffer[i] == '{') {
                    braceCount++;
                    qDebug() << "SERVER: Found nested { at" << i << "brace count:" << braceCount;
                }
                else if (buffer[i] == '}') {
                    braceCount--;
                    qDebug() << "SERVER: Found } at" << i << "brace count:" << braceCount;
                    if (braceCount == 0) {
                        endPos = i;
                        break;
                    }
                }
            }

            if (endPos > startPos) {
                QByteArray jsonData = buffer.mid(startPos, endPos - startPos + 1);
                qDebug() << "SERVER: Extracted JSON:" << QString::fromUtf8(jsonData);

                // Sprawdź, czy to prawidłowy JSON przed przetworzeniem
                QJsonParseError parseError;
                QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseError);
                if (parseError.error == QJsonParseError::NoError) {
                    qDebug() << "SERVER: Valid JSON detected, processing message";
                    processMessage(jsonData);
                } else {
                    qWarning() << "SERVER: Invalid JSON:" << parseError.errorString();
                }

                buffer.remove(0, endPos + 1);
                foundJson = true;

                qDebug() << "SERVER: Remaining buffer size after processing:"
                         << buffer.size();
            } else {
                qWarning() << "SERVER: No matching } found for { at position" << startPos;
            }
        } else {
            qDebug() << "SERVER: No JSON start marker found in buffer";
        }
    } while (foundJson);
}

void ClientSession::setUserId(quint32 id) {
    userId = id;
    ActiveSessions::getInstance().addSession(userId, this);
}

void ClientSession::sendUnreadFromUsers()
{
    if (!isAuthenticated || !userId) {
        qDebug() << "Cannot send unread users - not authenticated or no userId";
        return;
    }

    QVector<quint32> unreadUsers = dbManager->getUnreadMessagesUsers(userId);
    qDebug() << "Found" << unreadUsers.size() << "users with unread messages for user" << userId;

    QJsonArray usersArray;
    for (quint32 fromId : unreadUsers) {
        QJsonObject userObj;
        userObj["id"] = QString::number(fromId);
        usersArray.append(userObj);
        qDebug() << "Added user" << fromId << "to unread messages list";
    }

    QJsonObject response;
    response["type"] = Protocol::MessageType::UNREAD_FROM;
    response["users"] = usersArray;

    qDebug() << "Sending unread_from response:" << QJsonDocument(response).toJson();
    sendResponse(QJsonDocument(response).toJson());
}

void ClientSession::handleStatusUpdate(const QJsonObject& json) {
    QString newStatus = json["status"].toString();
    if (!newStatus.isEmpty() && userId > 0) {
        if (dbManager->updateUserStatus(userId, newStatus)) {
            sendFriendsStatusUpdate();
            qDebug() << "User" << userId << "status updated to:" << newStatus;
        } else {
            sendResponse(QJsonDocument(Protocol::MessageStructure::createError("Failed to update status")).toJson());
            qWarning() << "Failed to update status for user" << userId;
        }
    } else {
        sendResponse(QJsonDocument(Protocol::MessageStructure::createError("Invalid status update data")).toJson());
        qWarning() << "Invalid status update request received";
    }
}

void ClientSession::handleSearchUsers(const QJsonObject& json) {
    QString searchQuery = json["query"].toString();
    qDebug() << "Processing search users request with query:" << searchQuery;

    if (!searchQuery.isEmpty()) {
        auto results = dbManager->searchUsers(searchQuery, userId);

        QJsonArray usersArray;
        for (const auto& result : results) {
            QJsonObject userObj;
            userObj["id"] = QString::number(result.id);
            userObj["username"] = result.username;
            usersArray.append(userObj);
        }

        QJsonObject response{
            {"type", Protocol::MessageType::SEARCH_USERS_RESPONSE},
            {"users", usersArray},
            {"timestamp", QDateTime::currentMSecsSinceEpoch()}
        };

        qDebug() << "Sending search response with" << usersArray.size() << "results";
        sendResponse(QJsonDocument(response).toJson());
    } else {
        qWarning() << "Received empty search query";
        sendResponse(QJsonDocument(Protocol::MessageStructure::createError("Empty search query")).toJson());
    }
}

void ClientSession::handleRemoveFriend(const QJsonObject& json) {
    quint32 friendId = json["friend_id"].toInt();

    if (friendId > 0 && userId > 0) {
        if (dbManager->removeFriend(userId, friendId)) {
            ClientSession* friendSession = ActiveSessions::getInstance().getSession(friendId);

            handleFriendsListRequest();  // Dla inicjatora

            if (friendSession) {
                friendSession->handleFriendsListRequest(); // Dla usuniętego znajomego
            }

            QJsonObject response = Protocol::MessageStructure::createRemoveFriendResponse(true);
            sendResponse(QJsonDocument(response).toJson());

            if (friendSession) {
                QJsonObject friendRemovedNotification = Protocol::MessageStructure::createFriendRemovedNotification(userId);
                friendSession->sendResponse(QJsonDocument(friendRemovedNotification).toJson());
            }

            qDebug() << "Successfully removed friend" << friendId << "for user" << userId;
        } else {
            QJsonObject response = Protocol::MessageStructure::createRemoveFriendResponse(false);
            sendResponse(QJsonDocument(response).toJson());
            qWarning() << "Failed to remove friend" << friendId << "for user" << userId;
        }
    } else {
        sendResponse(QJsonDocument(Protocol::MessageStructure::createError(
                                       "Invalid friend removal request")).toJson());
        qWarning() << "Invalid friend removal request received";
    }
}

void ClientSession::handleGetLatestMessages(const QJsonObject& json) {
    quint32 friendId = json["friend_id"].toInt();
    int limit = json["limit"].toInt(Protocol::ChatHistory::MESSAGE_BATCH_SIZE);

    messages = dbManager->getLatestMessages(userId, friendId, limit);
    bool hasMore = dbManager->hasMoreHistory(userId, friendId, 0);

    QJsonObject response = prepareMessagesResponse();
    response["type"] = Protocol::MessageType::LATEST_MESSAGES_RESPONSE;
    response["has_more"] = hasMore;
    response["offset"] = messages.size();
    sendResponse(QJsonDocument(response).toJson());
}

void ClientSession::handleGetChatHistory(const QJsonObject& json) {
    quint32 friendId = json["friend_id"].toInt();
    int offset = json["offset"].toInt(0);

    messages = dbManager->getChatHistory(userId, friendId, offset);
    bool hasMore = dbManager->hasMoreHistory(userId, friendId, offset);

    QJsonObject response = prepareMessagesResponse();
    response["has_more"] = hasMore;
    response["offset"] = offset;
    sendResponse(QJsonDocument(response).toJson());
}

void ClientSession::handleGetMoreHistory(const QJsonObject& json) {
    quint32 friendId = json["friend_id"].toInt();
    int offset = json["offset"].toInt(0);

    messages = dbManager->getChatHistory(userId, friendId, offset);
    bool hasMore = dbManager->hasMoreHistory(userId, friendId, offset);

    QJsonObject response = prepareMessagesResponse();
    response["type"] = Protocol::MessageType::MORE_HISTORY_RESPONSE;
    response["has_more"] = hasMore;
    response["offset"] = offset;
    sendResponse(QJsonDocument(response).toJson());
}

void ClientSession::handleMessageRead(const QJsonObject& json) {
    quint32 friendId = json["friendId"].toInt();
    if (friendId > 0 && userId > 0) {
        if (dbManager->markChatAsRead(userId, friendId)) {
            sendResponse(QJsonDocument(Protocol::MessageStructure::createMessageReadResponse()).toJson());
            qDebug() << "Messages from user" << friendId << "marked as read for user" << userId;
        } else {
            sendResponse(QJsonDocument(Protocol::MessageStructure::createError("Failed to mark messages as read")).toJson());
            qWarning() << "Failed to mark messages as read from user" << friendId << "for user" << userId;
        }
    } else {
        sendResponse(QJsonDocument(Protocol::MessageStructure::createError("Invalid message read request")).toJson());
        qWarning() << "Invalid message read request received";
    }
}

void ClientSession::handleAddFriendRequest(const QJsonObject& json) {
    int targetUserId = json["user_id"].toInt();

    if (targetUserId <= 0 || userId <= 0) {
        sendResponse(QJsonDocument(Protocol::MessageStructure::createError("Invalid user ID")).toJson());
        return;
    }

    if (targetUserId == userId) {
        sendResponse(QJsonDocument(Protocol::MessageStructure::createError("Cannot send friend request to yourself")).toJson());
        return;
    }

    if (dbManager->sendFriendRequest(userId, targetUserId)) {
        QJsonObject response = Protocol::MessageStructure::createAddFriendResponse(true, "Friend request sent successfully");
        sendResponse(QJsonDocument(response).toJson());
        qDebug() << "Friend request sent successfully from user" << userId << "to user" << targetUserId;
    } else {
        QString targetUsername = dbManager->getUserUsername(targetUserId);

        QJsonObject response{
            {"type", Protocol::MessageType::INVITATION_ALREADY_EXISTS},
            {"user_id", targetUserId},
            {"username", targetUsername},
            {"status", "error"},
            {"error_code", "INVITATION_ALREADY_EXISTS"},
            {"message", "Invitation already sent to this user"},
            {"timestamp", QDateTime::currentMSecsSinceEpoch()}
        };

        sendResponse(QJsonDocument(response).toJson());
        qDebug() << "Error sending friend request: Friend request already sent";
    }
}

void ClientSession::handleGetReceivedInvitations() {
    auto invitations = dbManager->getReceivedInvitations(userId);
    QJsonArray invitationsArray;

    for (const auto& invitation : invitations) {
        QJsonObject invObj;
        invObj["request_id"] = invitation.requestId;
        invObj["user_id"] = QString::number(invitation.userId);
        invObj["username"] = invitation.username;
        invObj["status"] = invitation.status;
        invObj["timestamp"] = invitation.timestamp.toMSecsSinceEpoch();
        invitationsArray.append(invObj);
    }

    QJsonObject response{
        {"type", Protocol::MessageType::RECEIVED_INVITATIONS_RESPONSE},
        {"invitations", invitationsArray},
        {"timestamp", QDateTime::currentMSecsSinceEpoch()}
    };

    qDebug() << "Sending received invitations response with" << invitationsArray.size() << "invitations";
    sendResponse(QJsonDocument(response).toJson());
}

void ClientSession::handleGetSentInvitations() {
    auto invitations = dbManager->getSentInvitations(userId);
    QJsonArray invitationsArray;

    for (const auto& invitation : invitations) {
        QJsonObject invObj;
        invObj["request_id"] = invitation.requestId;
        invObj["user_id"] = QString::number(invitation.userId);
        invObj["username"] = invitation.username;
        invObj["status"] = invitation.status;
        invObj["timestamp"] = invitation.timestamp.toMSecsSinceEpoch();
        invitationsArray.append(invObj);
    }

    QJsonObject response{
        {"type", Protocol::MessageType::SENT_INVITATIONS_RESPONSE},
        {"invitations", invitationsArray},
        {"timestamp", QDateTime::currentMSecsSinceEpoch()}
    };

    qDebug() << "Sending sent invitations response with" << invitationsArray.size() << "invitations";
    sendResponse(QJsonDocument(response).toJson());
}

void ClientSession::handleCancelFriendRequest(const QJsonObject& json) {
    int requestId = json["request_id"].toInt();

    if (requestId <= 0 || userId <= 0) {
        sendResponse(QJsonDocument(Protocol::MessageStructure::createError(
                                       "Invalid request ID")).toJson());
        qWarning() << "Invalid cancel friend request received - requestId:" << requestId;
        return;
    }

    quint32 targetUserId = dbManager->getFriendRequestTargetUserId(userId, requestId);

    if (dbManager->cancelFriendInvitation(userId, requestId)) {
        QJsonObject response = Protocol::MessageStructure::createCancelFriendRequestResponse(
            true, "Friend request cancelled successfully");
        sendResponse(QJsonDocument(response).toJson());

        if (targetUserId > 0) {
            ClientSession* targetSession = ActiveSessions::getInstance().getSession(targetUserId);
            if (targetSession) {
                QJsonObject notification = Protocol::MessageStructure::createFriendRequestCancelledNotification(
                    requestId, userId);
                targetSession->sendResponse(QJsonDocument(notification).toJson());
            }
        }

        qDebug() << "Successfully cancelled friend request" << requestId
                 << "from user" << userId << "to user" << targetUserId;
    } else {
        sendResponse(QJsonDocument(Protocol::MessageStructure::createCancelFriendRequestResponse(
                                       false, "Failed to cancel friend request")).toJson());
        qWarning() << "Failed to cancel friend request" << requestId << "for user" << userId;
    }
}

void ClientSession::handleFriendRequestAccept(const QJsonObject& json) {
    int requestId = json["request_id"].toInt();

    if (requestId <= 0 || userId <= 0) {
        sendResponse(QJsonDocument(Protocol::MessageStructure::createError(
                                       "Invalid request ID")).toJson());
        return;
    }

    if (dbManager->acceptFriendInvitation(userId, requestId)) {
        QJsonObject response = Protocol::MessageStructure::createFriendRequestAcceptResponse(
            true, "Friend request accepted successfully");
        sendResponse(QJsonDocument(response).toJson());

        auto invitations = dbManager->getReceivedInvitations(userId);
        for (const auto& inv : invitations) {
            if (inv.requestId == requestId) {
                ClientSession* otherUserSession = ActiveSessions::getInstance().getSession(inv.userId);
                if (otherUserSession) {
                    QJsonObject notification = Protocol::MessageStructure::createFriendRequestAcceptedNotification(
                        userId,
                        dbManager->getUserUsername(userId)
                        );
                    otherUserSession->sendResponse(QJsonDocument(notification).toJson());
                    otherUserSession->handleFriendsListRequest();
                }
                break;
            }
        }

        handleFriendsListRequest();
    } else {
        sendResponse(QJsonDocument(Protocol::MessageStructure::createError(
                                       "Failed to accept friend request")).toJson());
    }
}

void ClientSession::handleFriendRequestReject(const QJsonObject& json) {
    int requestId = json["request_id"].toInt();

    if (requestId <= 0 || userId <= 0) {
        sendResponse(QJsonDocument(Protocol::MessageStructure::createError(
                                       "Invalid request ID")).toJson());
        return;
    }

    if (dbManager->rejectFriendInvitation(userId, requestId)) {
        QJsonObject response = Protocol::MessageStructure::createFriendRequestRejectResponse(
            true, "Friend request rejected successfully");
        sendResponse(QJsonDocument(response).toJson());
    } else {
        sendResponse(QJsonDocument(Protocol::MessageStructure::createError(
                                       "Failed to reject friend request")).toJson());
    }
}
