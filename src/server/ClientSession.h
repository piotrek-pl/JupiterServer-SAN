/**
 * @file ClientSession.h
 * @brief Declaration of the ClientSession class
 * @author piotrek-pl
 * @date 2025-01-19
 */

#ifndef CLIENTSESSION_H
#define CLIENTSESSION_H

#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QJsonObject>
#include <QHash>

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
    void sendFriendsStatusUpdate();
    void sendPendingMessages();
    void checkConnectionStatus();

private:
    void processMessage(const QByteArray& message);
    void sendResponse(const QByteArray& response);

    // Handler methods
    void handleLogin(const QJsonObject& json);
    void handleRegister(const QJsonObject& json);
    void handleLogout();
    void handleStatusRequest();
    void handleFriendsListRequest();
    void handleMessageRequest();
    void handlePing(const QJsonObject& message);
    void handleMessageAck(const QJsonObject& message);
    void handleSendMessage(const QJsonObject& json);

    // Helper methods
    QJsonObject prepareFriendsListResponse();
    QJsonObject prepareStatusResponse();
    QJsonObject prepareMessagesResponse();

    static constexpr int MAX_MISSED_PINGS = 3;

    // Member variables
    QTcpSocket* socket;
    DatabaseManager* dbManager;
    quint32 userId;
    bool isAuthenticated;
    QTimer statusUpdateTimer;
    QTimer messagesCheckTimer;
    QTimer pingTimer;
    qint64 lastPingTime;
    int missedPings;
    QHash<QString, QJsonObject> unconfirmedMessages;
};

#endif // CLIENTSESSION_H
