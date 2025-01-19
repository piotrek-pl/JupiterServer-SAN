#ifndef SERVER_H
#define SERVER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHash>
#include <memory>

class ClientSession;
class DatabaseManager;

class Server : public QObject
{
    Q_OBJECT
public:
    explicit Server(QObject *parent = nullptr);
    ~Server();

    bool start(quint16 port = 1234);
    void stop();

private slots:
    void handleNewConnection();
    void handleClientDisconnected();

private:
    std::unique_ptr<QTcpServer> m_server;
    std::unique_ptr<DatabaseManager> m_dbManager;
    QHash<QTcpSocket*, ClientSession*> m_clientSessions;  // Zmienione z QMap na QHash i unique_ptr na zwykły wskaźnik
};

#endif // SERVER_H
