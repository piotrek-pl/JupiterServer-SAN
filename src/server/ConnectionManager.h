// src/server/ConnectionManager.h
#pragma once

#include <QObject>
#include <QMap>
#include <QSharedPointer>
#include <QList>
#include "../database/DatabaseManager.h"
#include "ClientSession.h"

class ConnectionManager : public QObject {
    Q_OBJECT
public:
    explicit ConnectionManager(QSharedPointer<DatabaseManager> dbManager, QObject* parent = nullptr);
    ~ConnectionManager() = default;

    void addClient(QTcpSocket* socket);
    void removeClient(int userId);
    ClientSession* getClient(int userId);
    QList<int> getOnlineUsers() const;

    // Nowe metody
    QList<ClientSession*> getFriendConnections(int userId);
    QList<ClientSession*> getConnectionsForUser(int userId);

signals:
    void clientConnected(int userId);
    void clientDisconnected(int userId);
    void messageReceived(int fromUserId, int toUserId, const QString& message);

private:
    QMap<int, ClientSession*> clients;
    QSharedPointer<DatabaseManager> m_dbManager;
};
