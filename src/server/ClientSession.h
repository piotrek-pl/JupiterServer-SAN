#ifndef CLIENTSESSION_H
#define CLIENTSESSION_H

#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QJsonObject>

class DatabaseManager;

class ClientSession : public QObject
{
    Q_OBJECT
public:
    explicit ClientSession(QTcpSocket* socket, DatabaseManager* dbManager, QObject *parent = nullptr);
    ~ClientSession();

private slots:
    void handleReadyRead();
    void handleError(QAbstractSocket::SocketError socketError);
    // Nowe sloty do obsługi cyklicznych zapytań
    void sendFriendsStatusUpdate();
    void sendPendingMessages();

private:
    void processMessage(const QByteArray& message);
    void sendResponse(const QByteArray& response);
    void handleStatusRequest();
    void handleFriendsListRequest();
    void handleMessageRequest();

    // Metody pomocnicze do przygotowania odpowiedzi
    QJsonObject prepareFriendsListResponse();
    QJsonObject prepareStatusResponse();
    QJsonObject prepareMessagesResponse();

    QTcpSocket* m_socket;
    DatabaseManager* m_dbManager;
    quint32 m_userId;
    bool m_isAuthenticated;
    QTimer m_statusUpdateTimer;    // Timer do aktualizacji statusu
    QTimer m_messagesCheckTimer;   // Timer do sprawdzania nowych wiadomości
};

#endif // CLIENTSESSION_H
