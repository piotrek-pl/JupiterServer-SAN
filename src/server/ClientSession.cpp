/**
 * @file ClientSession.cpp
 * @brief Implementation of the ClientSession class
 * @author piotrek-pl
 * @date 2025-01-22 09:34:34
 */

#include "ClientSession.h"
#include "database/DatabaseManager.h"
#include "Protocol.h"
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
    if (!socket->bytesAvailable()) return;

    buffer.append(socket->readAll());
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
        QString newStatus = json["status"].toString();
        if (!newStatus.isEmpty() && userId > 0) {
            if (dbManager->updateUserStatus(userId, newStatus)) {
                QJsonObject response{
                    {"type", Protocol::MessageType::STATUS_UPDATE},
                    {"status", newStatus},
                    {"timestamp", QDateTime::currentMSecsSinceEpoch()}
                };
                sendResponse(QJsonDocument(response).toJson());
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
    else if (type == Protocol::MessageType::SEARCH_USERS) {
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
    else if (type == Protocol::MessageType::REMOVE_FRIEND) {
        quint32 friendId = json["friend_id"].toInt();

        if (friendId > 0 && userId > 0) {
            if (dbManager->removeFriend(userId, friendId)) {
                // Najpierw znajdujemy sesję znajomego
                ClientSession* friendSession = ActiveSessions::getInstance().getSession(friendId);

                // Najpierw wysyłamy obu użytkownikom odświeżenie listy
                handleFriendsListRequest();  // Dla inicjatora

                if (friendSession) {
                    friendSession->handleFriendsListRequest(); // Dla usuniętego znajomego
                }

                // Potem wysyłamy potwierdzenia
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
    else if (type == Protocol::MessageType::GET_LATEST_MESSAGES) {
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
    else if (type == Protocol::MessageType::GET_CHAT_HISTORY) {
        quint32 friendId = json["friend_id"].toInt();
        int offset = json["offset"].toInt(0);

        messages = dbManager->getChatHistory(userId, friendId, offset);
        bool hasMore = dbManager->hasMoreHistory(userId, friendId, offset);

        QJsonObject response = prepareMessagesResponse();
        response["has_more"] = hasMore;
        response["offset"] = offset;
        sendResponse(QJsonDocument(response).toJson());
    }
    else if (type == Protocol::MessageType::GET_MORE_HISTORY) {
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
    else if (type == Protocol::MessageType::SEND_MESSAGE) {
        handleSendMessage(json);
    }
    else if (type == Protocol::MessageType::MESSAGE_READ) {
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
    else if (type == Protocol::MessageType::ADD_FRIEND_REQUEST) {
        int targetUserId = json["user_id"].toInt();
        qDebug() << "Processing add friend request from user" << userId << "to user" << targetUserId;

        if (targetUserId <= 0 || userId <= 0) {
            sendResponse(QJsonDocument(Protocol::MessageStructure::createError("Invalid user ID")).toJson());
            return;
        }

        if (targetUserId == userId) {
            sendResponse(QJsonDocument(Protocol::MessageStructure::createError("Cannot send friend request to yourself")).toJson());
            return;
        }

        // Przekazujemy żądanie do DatabaseManager
        if (dbManager->sendFriendRequest(userId, targetUserId)) {
            QJsonObject response;
            response["type"] = Protocol::MessageType::ADD_FRIEND_RESPONSE;
            response["status"] = "success";
            response["message"] = "Friend request sent successfully";
            response["timestamp"] = QDateTime::currentMSecsSinceEpoch();
            sendResponse(QJsonDocument(response).toJson());

            qDebug() << "Friend request sent successfully from user" << userId << "to user" << targetUserId;
        } else {
            sendResponse(QJsonDocument(Protocol::MessageStructure::createError("Failed to send friend request")).toJson());
            qWarning() << "Failed to send friend request from user" << userId << "to user" << targetUserId;
        }
    }
    else if (type == Protocol::MessageType::GET_RECEIVED_INVITATIONS) {
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
    else if (type == Protocol::MessageType::GET_SENT_INVITATIONS) {
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

    if (username.isEmpty() || password.isEmpty()) {
        state = Protocol::SessionState::INITIAL;
        sendResponse(QJsonDocument(Protocol::MessageStructure::createError("Invalid credentials")).toJson());
        return;
    }

    QSqlDatabase sessionDb = QSqlDatabase::database(sessionConnectionName);
    if (!sessionDb.isOpen()) {
        qWarning() << "Session database connection is not open! Attempting to reopen...";
        if (!dbManager->cloneConnectionForThread(sessionConnectionName)) {
            sendResponse(QJsonDocument(Protocol::MessageStructure::createError("Database connection error")).toJson());
            return;
        }
    }

    if (dbManager->authenticateUser(username, password, userId)) {
        setUserId(userId);
        state = Protocol::SessionState::AUTHENTICATED;
        isAuthenticated = true;
        statusUpdateTimer.start();

        dbManager->updateUserStatus(userId, "online");

        // Wysyłamy odpowiedź o udanym logowaniu
        QJsonObject response{
            {"type", Protocol::MessageType::LOGIN_RESPONSE},
            {"status", "success"},
            {"userId", static_cast<int>(userId)},
            {"timestamp", QDateTime::currentMSecsSinceEpoch()}
        };
        sendResponse(QJsonDocument(response).toJson());

        // Wysyłamy informację o nieprzeczytanych wiadomościach
        sendUnreadFromUsers();

        // Wysyłamy listę znajomych (zakładam, że jest realizowane w innym miejscu)
        handleFriendsListRequest();

        qDebug() << "User" << username << "logged in successfully";
    } else {
        state = Protocol::SessionState::INITIAL;
        sendResponse(QJsonDocument(Protocol::MessageStructure::createError("Authentication failed")).toJson());
        qDebug() << "Failed login attempt for user:" << username;
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

    if (dbManager->registerUser(username, password)) {
        QJsonObject response{
            {"type", Protocol::MessageType::REGISTER_RESPONSE},
            {"status", "success"},
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

    lastPingTime = QDateTime::currentMSecsSinceEpoch();
    missedPings = 0;
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
    bool foundJson;
    do {
        foundJson = false;
        int startPos = buffer.indexOf('{');
        if (startPos >= 0) {
            int endPos = -1;
            int braceCount = 1;

            for (int i = startPos + 1; i < buffer.size(); ++i) {
                if (buffer[i] == '{') braceCount++;
                else if (buffer[i] == '}') {
                    braceCount--;
                    if (braceCount == 0) {
                        endPos = i;
                        break;
                    }
                }
            }

            if (endPos > startPos) {
                QByteArray jsonData = buffer.mid(startPos, endPos - startPos + 1);
                qDebug() << "SERVER: Processing JSON of size:" << jsonData.size();

                processMessage(jsonData);
                buffer.remove(0, endPos + 1);
                foundJson = true;
            }
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
