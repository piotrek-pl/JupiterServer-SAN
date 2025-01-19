// src/server/NotificationService.h
#pragma once

#include <QObject>
#include <QJsonObject>
#include <QSharedPointer>
#include "ConnectionManager.h"
#include "ClientSession.h"

class NotificationService : public QObject {
    Q_OBJECT
public:
    explicit NotificationService(QSharedPointer<ConnectionManager> connectionManager, QObject* parent = nullptr);

public slots:
    void notifyStatusChange(int userId, const QString& status);
    void notifyNewMessage(const Message& message);
    void notifyFriendListUpdate(int userId, int friendId, bool added);

private:
    void sendNotification(ClientSession* session, const QJsonObject& notification); // Zmieniony typ parametru
    QSharedPointer<ConnectionManager> connectionManager;
};
