// src/server/NotificationService.cpp
#include "NotificationService.h"
#include <QJsonDocument>
#include <QDebug>

NotificationService::NotificationService(QSharedPointer<ConnectionManager> connectionManager, QObject* parent)
    : QObject(parent)
    , connectionManager(connectionManager)
{
}

void NotificationService::notifyStatusChange(int userId, const QString& status)
{
    QJsonObject notification{
        {"type", "status_change"},
        {"user_id", userId},
        {"status", status}
    };

    for (auto socket : connectionManager->getFriendConnections(userId)) {
        sendNotification(socket, notification);
    }
}

void NotificationService::notifyNewMessage(const Message& message)
{
    QJsonObject notification{
        {"type", "new_message"},
        {"sender_id", message.getSenderId()},
        {"message", message.getContent()}
    };

    for (auto session : connectionManager->getConnectionsForUser(message.getReceiverId())) {
        sendNotification(session, notification);
    }
}

void NotificationService::notifyFriendListUpdate(int userId, int friendId, bool added)
{
    QJsonObject notification{
        {"type", "friend_list_update"},
        {"friend_id", friendId},
        {"action", added ? "added" : "removed"}
    };

    for (auto session : connectionManager->getConnectionsForUser(userId)) {
        sendNotification(session, notification);
    }
}

void NotificationService::sendNotification(ClientSession* session, const QJsonObject& notification)
{
    if (session) {
        session->sendResponse(notification);
    }
}
