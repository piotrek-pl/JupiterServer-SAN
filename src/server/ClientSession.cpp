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
{
    qDebug() << "ClientSession constructor called";

    // Tworzymy unikalną nazwę połączenia używając adresu obiektu sesji
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
    qDebug() << "ClientSession destructor called";

    // Zamykamy i usuwamy połączenie z bazą danych
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

    // Zawsze akceptuj PING/PONG
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

    // Dla pozostałych wiadomości sprawdź stan
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
    else if (type == Protocol::MessageType::MESSAGE_ACK) {
        handleMessageAck(json);
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
                // Przygotuj i wyślij odpowiedź potwierdzającą zmianę statusu
                QJsonObject response{
                    {"type", Protocol::MessageType::STATUS_UPDATE},
                    {"status", newStatus},
                    {"timestamp", QDateTime::currentMSecsSinceEpoch()}
                };
                sendResponse(QJsonDocument(response).toJson());

                // Od razu wyślij aktualizację statusu do wszystkich znajomych
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
        state = Protocol::SessionState::INITIAL;
        sendResponse(QJsonDocument(Protocol::MessageStructure::createError("Invalid credentials")).toJson());
        return;
    }

    // Użyj połączenia specyficznego dla tej sesji
    QSqlDatabase sessionDb = QSqlDatabase::database(sessionConnectionName);
    if (!sessionDb.isOpen()) {
        qWarning() << "Session database connection is not open! Attempting to reopen...";
        if (!dbManager->cloneConnectionForThread(sessionConnectionName)) {
            sendResponse(QJsonDocument(Protocol::MessageStructure::createError("Database connection error")).toJson());
            return;
        }
    }

    if (dbManager->authenticateUser(username, password, userId)) {
        state = Protocol::SessionState::AUTHENTICATED;
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

void ClientSession::handlePing(const QJsonObject& message)
{
    qDebug() << "SERVER: Received PING from client at"
             << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");

    // Tylko aktualizujemy czas ostatniego pinga
    lastPingTime = QDateTime::currentMSecsSinceEpoch();
    missedPings = 0;

    // NIE wysyłamy odpowiedzi PONG
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
    if (!socket || !socket->isValid() || socket->state() != QAbstractSocket::ConnectedState) {
        return;
    }

    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();

    // Wysyłamy ping do klienta
    QJsonObject pingMessage = Protocol::MessageStructure::createPing();
    pingMessage["timestamp"] = currentTime;

    qDebug() << "SERVER: Sending PING at"
             << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz")
             << "with timestamp:" << currentTime;

    sendResponse(QJsonDocument(pingMessage).toJson());

    // Sprawdzamy odpowiedzi na pingi
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
        int friendId = static_cast<int>(friend_.first);
        friendObj["id"] = friendId;
        friendObj["username"] = friend_.second;

        // Pobierz status znajomego
        bool isOnline = dbManager->getUserStatus(friendId, sessionConnectionName);
        friendObj["status"] = isOnline ? Protocol::UserStatus::ONLINE : Protocol::UserStatus::OFFLINE;

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

void ClientSession::sendPendingMessages()
{
    if (isAuthenticated) {
        QJsonObject response = prepareMessagesResponse();
        if (!response["messages"].toArray().isEmpty()) {
            sendResponse(QJsonDocument(response).toJson());
        }
    }
}
