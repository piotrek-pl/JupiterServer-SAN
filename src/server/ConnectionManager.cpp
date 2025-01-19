// src/server/ConnectionManager.cpp
#include "ConnectionManager.h"
#include <QDebug>

ConnectionManager::ConnectionManager(QSharedPointer<DatabaseManager> dbManager, QObject* parent)
    : QObject(parent)
    , m_dbManager(dbManager)
{
}

void ConnectionManager::addClient(QTcpSocket* socket)
{
    auto session = new ClientSession(socket, m_dbManager, this);

    connect(session, &ClientSession::userLoggedIn,
            this, [this, session](int userId) {
                clients[userId] = session;
                emit clientConnected(userId);
            });

    connect(session, &ClientSession::userLoggedOut,
            this, [this](int userId) {
                removeClient(userId);
            });

    connect(session, &ClientSession::messageReceived,
            this, &ConnectionManager::messageReceived);
}

void ConnectionManager::removeClient(int userId)
{
    if (clients.contains(userId)) {
        auto session = clients[userId];
        clients.remove(userId);
        session->deleteLater();
        emit clientDisconnected(userId);
    }
}

ClientSession* ConnectionManager::getClient(int userId)
{
    return clients.value(userId, nullptr);
}

QList<int> ConnectionManager::getOnlineUsers() const
{
    return clients.keys();
}

QList<ClientSession*> ConnectionManager::getFriendConnections(int userId)
{
    QList<ClientSession*> friendSessions;

    // Pobierz listę znajomych użytkownika
    auto friendsList = m_dbManager->getFriendsList(userId);

    // Dla każdego znajomego sprawdź, czy jest online
    for (const auto& friend_ : friendsList) {
        int friendId = friend_.first;
        if (clients.contains(friendId)) {
            friendSessions.append(clients[friendId]);
        }
    }

    return friendSessions;
}

QList<ClientSession*> ConnectionManager::getConnectionsForUser(int userId)
{
    QList<ClientSession*> sessions;

    // Jeśli użytkownik jest online, zwróć jego sesję
    if (clients.contains(userId)) {
        sessions.append(clients[userId]);
    }

    return sessions;
}
