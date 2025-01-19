/**
 * @file ClientSession.cpp
 * @brief Implementation of the ClientSession class
 * @author piotrek-pl
 * @date 2025-01-19 16:53:42
 */

#include "ClientSession.h"
#include "database/DatabaseManager.h"
#include "Protocol.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUuid>
#include <QDateTime>

ClientSession::ClientSession(QTcpSocket* socket, DatabaseManager* dbManager, QObject *parent)
    : QObject(parent)
    , socket(socket)
    , dbManager(dbManager)
    , userId(0)
    , isAuthenticated(false)
    , lastPingTime(QDateTime::currentMSecsSinceEpoch())
    , missedPings(0)
{
    connect(socket, &QTcpSocket::readyRead,
            this, &ClientSession::handleReadyRead);
    connect(socket, &QTcpSocket::errorOccurred,
            this, &ClientSession::handleError);

    // Konfiguracja timerów
    statusUpdateTimer.setInterval(Protocol::Timeouts::STATUS_UPDATE);
    connect(&statusUpdateTimer, &QTimer::timeout,
            this, &ClientSession::sendFriendsStatusUpdate);

    messagesCheckTimer.setInterval(Protocol::Timeouts::REQUEST);
    connect(&messagesCheckTimer, &QTimer::timeout,
            this, &ClientSession::sendPendingMessages);

    pingTimer.setInterval(Protocol::Timeouts::PING);
    connect(&pingTimer, &QTimer::timeout,
            this, &ClientSession::checkConnectionStatus);
    pingTimer.start();

    qDebug() << "New client session created";
}

ClientSession::~ClientSession()
{
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
    QByteArray data = socket->readAll();
    if (data.isEmpty()) {
        qWarning() << "Received empty data from client";
        return;
    }

    processMessage(data);
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

    // Informujemy bazę danych o rozłączeniu użytkownika
    if (isAuthenticated && userId > 0) {
        dbManager->updateUserStatus(userId, "offline");
    }
}

void ClientSession::processMessage(const QByteArray& message)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(message, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "Failed to parse message:" << parseError.errorString();
        sendResponse(QJsonDocument(Protocol::MessageStructure::createError("Invalid JSON format")).toJson());
        return;
    }

    QJsonObject json = doc.object();
    QString type = json["type"].toString();

    qDebug() << "Received message type:" << type;

    if (!isAuthenticated && type != Protocol::MessageType::LOGIN && type != Protocol::MessageType::REGISTER) {
        sendResponse(QJsonDocument(Protocol::MessageStructure::createError("Not authenticated")).toJson());
        return;
    }

    if (type == Protocol::MessageType::LOGIN) {
        handleLogin(json);
    }
    else if (type == Protocol::MessageType::REGISTER) {
        handleRegister(json);
    }
    else if (type == Protocol::MessageType::PING) {
        handlePing(json);
    }
    else if (type == Protocol::MessageType::MESSAGE_ACK) {
        handleMessageAck(json);
    }
    else if (type == Protocol::MessageType::GET_FRIENDS_LIST) {
        handleFriendsListRequest();
    }
    else if (type == Protocol::MessageType::GET_STATUS) {
        handleStatusRequest();
    }
    else if (type == Protocol::MessageType::GET_MESSAGES) {
        handleMessageRequest();
    }
    else if (type == Protocol::MessageType::SEND_MESSAGE) {
        handleSendMessage(json);
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
        sendResponse(QJsonDocument(Protocol::MessageStructure::createError("Invalid credentials")).toJson());
        return;
    }

    if (dbManager->authenticateUser(username, password, userId)) {
        isAuthenticated = true;
        statusUpdateTimer.start();
        messagesCheckTimer.start();

        dbManager->updateUserStatus(userId, "online");

        QJsonObject response{
            {"type", Protocol::MessageType::LOGIN_RESPONSE},
            {"status", "success"},
            {"userId", static_cast<int>(userId)},
            {"timestamp", QDateTime::currentMSecsSinceEpoch()}
        };
        sendResponse(QJsonDocument(response).toJson());

        qDebug() << "User" << username << "logged in successfully";
    } else {
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
        messagesCheckTimer.stop();

        QJsonObject response{
            {"type", "logout_response"},
            {"status", "success"},
            {"timestamp", QDateTime::currentMSecsSinceEpoch()}
        };
        sendResponse(QJsonDocument(response).toJson());

        qDebug() << "User logged out successfully";
    }
}

// ClientSession.cpp
void ClientSession::handlePing(const QJsonObject& message)
{
    qDebug() << "Handling ping message:" << message;

    lastPingTime = QDateTime::currentMSecsSinceEpoch();
    missedPings = 0;

    // Sprawdź czy wiadomość ping zawiera timestamp
    if (!message.contains("timestamp")) {
        qWarning() << "Ping message missing timestamp";
        sendResponse(QJsonDocument(Protocol::MessageStructure::createError("Invalid ping message")).toJson());
        return;
    }

    qint64 timestamp = message["timestamp"].toInteger();
    QJsonObject pong = Protocol::MessageStructure::createPong(timestamp);

    qDebug() << "Sending pong response:" << pong;

    sendResponse(QJsonDocument(pong).toJson());
}

void ClientSession::handleMessageAck(const QJsonObject& message)
{
    QString messageId = message["message_id"].toString();
    if (unconfirmedMessages.contains(messageId)) {
        unconfirmedMessages.remove(messageId);
        qDebug() << "Message" << messageId << "acknowledged";
    }
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

    if (dbManager->storeMessage(userId, receiverId, content)) {
        QJsonObject response = Protocol::MessageStructure::createMessageAck(messageId);
        sendResponse(QJsonDocument(response).toJson());
        qDebug() << "Message" << messageId << "stored successfully";
    } else {
        sendResponse(QJsonDocument(Protocol::MessageStructure::createError("Failed to store message")).toJson());
        qWarning() << "Failed to store message" << messageId;
    }
}

void ClientSession::checkConnectionStatus()
{
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    if (currentTime - lastPingTime > Protocol::Timeouts::CONNECTION) {
        missedPings++;
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

void ClientSession::handleMessageRequest()
{
    QJsonObject response = prepareMessagesResponse();
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
        friendObj["id"] = static_cast<int>(friend_.first);
        friendObj["username"] = friend_.second;
        friendsArray.append(friendObj);
    }

    response["friends"] = friendsArray;
    response["timestamp"] = QDateTime::currentMSecsSinceEpoch();
    return response;
}

QJsonObject ClientSession::prepareStatusResponse()
{
    return Protocol::MessageStructure::createStatusUpdate("online");
}

QJsonObject ClientSession::prepareMessagesResponse()
{
    QJsonObject response;
    response["type"] = Protocol::MessageType::PENDING_MESSAGES;

    QJsonArray messagesArray;
    auto messages = dbManager->getChatHistory(userId, 0, 10);

    for (const auto& msg : messages) {
        QJsonObject msgObj;
        msgObj["sender"] = msg.first;
        msgObj["content"] = msg.second;
        messagesArray.append(msgObj);
    }

    response["messages"] = messagesArray;
    response["timestamp"] = QDateTime::currentMSecsSinceEpoch();
    return response;
}

void ClientSession::sendResponse(const QByteArray& response)
{
    if (!socket || !socket->isValid()) {
        qWarning() << "Attempting to send response through invalid socket";
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(response);
    QJsonObject json = doc.object();

    if (json["type"] == Protocol::MessageType::SEND_MESSAGE) {
        QString messageId = QUuid::createUuid().toString();
        json["message_id"] = messageId;
        unconfirmedMessages[messageId] = json;

        QTimer::singleShot(Protocol::Timeouts::REQUEST, [this, messageId]() {
            if (unconfirmedMessages.contains(messageId)) {
                qDebug() << "Resending unconfirmed message:" << messageId;
                sendResponse(QJsonDocument(unconfirmedMessages[messageId]).toJson());
            }
        });
    }

    socket->write(QJsonDocument(json).toJson());
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

void ClientSession::sendPendingMessages()
{
    if (isAuthenticated) {
        QJsonObject response = prepareMessagesResponse();
        if (!response["messages"].toArray().isEmpty()) {
            sendResponse(QJsonDocument(response).toJson());
        }
    }
}
