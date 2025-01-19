// NotificationManager.cpp
#include "NotificationManager.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

NotificationManager& NotificationManager::getInstance()
{
    static NotificationManager instance;
    return instance;
}

NotificationManager::NotificationManager(QObject *parent)
    : QObject(parent)
    , webSocket(new QWebSocket)
    , connected(false)
{
    setupConnections();
}

NotificationManager::~NotificationManager() = default;

void NotificationManager::setupConnections()
{
    connect(webSocket.data(), &QWebSocket::connected,
            this, &NotificationManager::onConnected);
    connect(webSocket.data(), &QWebSocket::disconnected,
            this, &NotificationManager::onDisconnected);
    connect(webSocket.data(), &QWebSocket::textMessageReceived,
            this, &NotificationManager::onTextMessageReceived);
    connect(webSocket.data(), &QWebSocket::errorOccurred,
            this, &NotificationManager::onErrorOccurred);
}

void NotificationManager::connectToServer(const QString& url)
{
    serverUrl = url;
    webSocket->open(QUrl(url));
}

void NotificationManager::disconnectFromServer()
{
    webSocket->close();
}

bool NotificationManager::isConnected() const
{
    return connected;
}

void NotificationManager::onConnected()
{
    connected = true;
    emit connectionStatusChanged(true);
    qDebug() << "WebSocket connected";
}

void NotificationManager::onDisconnected()
{
    connected = false;
    emit connectionStatusChanged(false);
    qDebug() << "WebSocket disconnected";
}

void NotificationManager::onTextMessageReceived(const QString& message)
{
    handleMessage(message);
}

void NotificationManager::handleMessage(const QString& message)
{
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (doc.isNull() || !doc.isObject()) {
        qWarning() << "Invalid JSON message received";
        return;
    }

    QJsonObject obj = doc.object();
    QString type = obj["type"].toString();

    if (type == "userStatus") {
        emit userStatusChanged(
            obj["userId"].toString(),
            obj["status"].toString()
            );
    }
    else if (type == "newMessage") {
        emit newMessageReceived(
            obj["from"].toString(),
            obj["content"].toString()
            );
    }
    else if (type == "friendList") {
        emit friendListChanged(
            obj["userId"].toString(),
            obj["added"].toBool()
            );
    }
}

void NotificationManager::onErrorOccurred(QAbstractSocket::SocketError error)  // Poprawiona nazwa
{
    qWarning() << "WebSocket error:" << error
               << webSocket->errorString();
}
