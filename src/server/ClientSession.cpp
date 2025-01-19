#include "ClientSession.h"
#include "database/DatabaseManager.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

ClientSession::ClientSession(QTcpSocket* socket, DatabaseManager* dbManager, QObject *parent)
    : QObject(parent)
    , m_socket(socket)
    , m_dbManager(dbManager)
    , m_userId(0)
    , m_isAuthenticated(false)
{
    connect(m_socket, &QTcpSocket::readyRead,
            this, &ClientSession::handleReadyRead);
    connect(m_socket, &QTcpSocket::errorOccurred,
            this, &ClientSession::handleError);

    // Konfiguracja timerów
    m_statusUpdateTimer.setInterval(3000); // 3 sekundy
    connect(&m_statusUpdateTimer, &QTimer::timeout,
            this, &ClientSession::sendFriendsStatusUpdate);

    m_messagesCheckTimer.setInterval(1000); // 1 sekunda
    connect(&m_messagesCheckTimer, &QTimer::timeout,
            this, &ClientSession::sendPendingMessages);
}

ClientSession::~ClientSession()
{
    m_statusUpdateTimer.stop();
    m_messagesCheckTimer.stop();
    if (m_socket) {
        m_socket->disconnectFromHost();
    }
}

void ClientSession::handleReadyRead()
{
    QByteArray data = m_socket->readAll();
    processMessage(data);
}

void ClientSession::handleError(QAbstractSocket::SocketError socketError)
{
    qWarning() << "Socket error:" << socketError
               << "Error string:" << m_socket->errorString();

    // Możemy tu dodać obsługę różnych typów błędów
    switch (socketError) {
    case QAbstractSocket::RemoteHostClosedError:
        qDebug() << "Remote host closed connection";
        break;
    case QAbstractSocket::HostNotFoundError:
        qDebug() << "Host not found";
        break;
    default:
        qDebug() << "Unknown socket error occurred";
    }
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

QJsonObject ClientSession::prepareStatusResponse()
{
    QJsonObject response;
    response["type"] = "status_response";
    response["status"] = "online"; // Tu możemy dodać więcej logiki statusu
    return response;
}

void ClientSession::sendResponse(const QByteArray& response)
{
    if (m_socket && m_socket->isValid()) {
        m_socket->write(response);
        m_socket->flush();
    }
}

void ClientSession::processMessage(const QByteArray& message)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(message, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "Failed to parse message:" << parseError.errorString();
        return;
    }

    QJsonObject json = doc.object();
    QString type = json["type"].toString();

    if (!m_isAuthenticated && type != "login" && type != "register") {
        QJsonObject response{
            {"type", "error"},
            {"message", "Not authenticated"}
        };
        sendResponse(QJsonDocument(response).toJson());
        return;
    }

    // Obsługa różnych typów zapytań
    if (type == "login") {
        QString username = json["username"].toString();
        QString password = json["password"].toString();

        if (m_dbManager->authenticateUser(username, password, m_userId)) {
            m_isAuthenticated = true;
            // Po udanym logowaniu startujemy timery
            m_statusUpdateTimer.start();
            m_messagesCheckTimer.start();

            QJsonObject response{
                {"type", "login_response"},
                {"status", "success"},
                {"userId", static_cast<int>(m_userId)}
            };
            sendResponse(QJsonDocument(response).toJson());
        }
    }
    else if (type == "get_friends_list") {
        handleFriendsListRequest();
    }
    else if (type == "get_status") {
        handleStatusRequest();
    }
    else if (type == "get_messages") {
        handleMessageRequest();
    }
}

void ClientSession::handleFriendsListRequest()
{
    QJsonObject response = prepareFriendsListResponse();
    sendResponse(QJsonDocument(response).toJson());
}

QJsonObject ClientSession::prepareFriendsListResponse()
{
    QJsonObject response;
    response["type"] = "friends_list_response";

    QJsonArray friendsArray;
    auto friendsList = m_dbManager->getFriendsList(m_userId);

    for (const auto& friend_ : friendsList) {
        QJsonObject friendObj;
        friendObj["id"] = static_cast<int>(friend_.first);
        friendObj["username"] = friend_.second;
        friendsArray.append(friendObj);
    }

    response["friends"] = friendsArray;
    return response;
}

void ClientSession::sendFriendsStatusUpdate()
{
    if (m_isAuthenticated) {
        QJsonObject response = prepareFriendsListResponse();
        response["type"] = "friends_status_update";
        sendResponse(QJsonDocument(response).toJson());
    }
}

void ClientSession::sendPendingMessages()
{
    if (m_isAuthenticated) {
        QJsonObject response = prepareMessagesResponse();
        if (!response.isEmpty()) {
            sendResponse(QJsonDocument(response).toJson());
        }
    }
}

QJsonObject ClientSession::prepareMessagesResponse()
{
    QJsonObject response;
    response["type"] = "pending_messages";

    QJsonArray messagesArray;
    auto messages = m_dbManager->getChatHistory(m_userId, 0, 10); // 0 oznacza wszystkie nowe wiadomości

    for (const auto& msg : messages) {
        QJsonObject msgObj;
        msgObj["sender"] = msg.first;
        msgObj["content"] = msg.second;
        messagesArray.append(msgObj);
    }

    if (!messagesArray.isEmpty()) {
        response["messages"] = messagesArray;
    }

    return response;
}
