#include "Server.h"
#include "ClientSession.h"
#include "database/DatabaseManager.h"
#include <QDebug>

Server::Server(QObject *parent)
    : QObject(parent)
    , m_server(std::make_unique<QTcpServer>(this))
    , m_dbManager(std::make_unique<DatabaseManager>())
{
}

Server::~Server()
{
    // Czyścimy sesje klientów
    qDeleteAll(m_clientSessions);
    m_clientSessions.clear();
}

bool Server::start(quint16 port)
{
    if (!m_server->listen(QHostAddress::Any, port)) {
        qCritical() << "Server failed to start. Error:" << m_server->errorString();
        return false;
    }

    connect(m_server.get(), &QTcpServer::newConnection,
            this, &Server::handleNewConnection);

    qInfo() << "Server is listening on port" << port;
    return true;
}

void Server::stop()
{
    if (m_server->isListening()) {
        m_server->close();
        qDeleteAll(m_clientSessions);
        m_clientSessions.clear();
        qInfo() << "Server stopped";
    }
}

void Server::handleNewConnection()
{
    QTcpSocket *clientSocket = m_server->nextPendingConnection();
    if (!clientSocket) {
        return;
    }

    qInfo() << "New client connected:" << clientSocket->peerAddress().toString();

    // Tworzymy nową sesję i ustawiamy jej rodzica na clientSocket
    ClientSession* session = new ClientSession(clientSocket, m_dbManager.get(), clientSocket);

    connect(clientSocket, &QTcpSocket::disconnected,
            this, &Server::handleClientDisconnected);

    m_clientSessions.insert(clientSocket, session);
}

void Server::handleClientDisconnected()
{
    QTcpSocket *clientSocket = qobject_cast<QTcpSocket*>(sender());
    if (clientSocket) {
        qInfo() << "Client disconnected:" << clientSocket->peerAddress().toString();

        // Sesja zostanie usunięta automatycznie przez system rodzica Qt
        m_clientSessions.remove(clientSocket);
    }
}
