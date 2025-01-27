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
#include "database/DatabaseManager.h"

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
    void processBuffer();
    void sendUnreadFromUsers();
    void handleStatusUpdate(const QJsonObject& json);
    void handleSearchUsers(const QJsonObject& json);
    void handleRemoveFriend(const QJsonObject& json);
    void handleGetLatestMessages(const QJsonObject& json);
    void handleGetChatHistory(const QJsonObject& json);
    void handleGetMoreHistory(const QJsonObject& json);
    void handleMessageRead(const QJsonObject& json);
    void handleAddFriendRequest(const QJsonObject& json);
    void handleGetReceivedInvitations();
    void handleGetSentInvitations();
    void handleCancelFriendRequest(const QJsonObject& json);
    void handleFriendRequestAccept(const QJsonObject& json);
    void handleFriendRequestReject(const QJsonObject& json);

    void setUserId(quint32 id);

    // Helper methods
    QJsonObject prepareFriendsListResponse();
    QJsonObject prepareStatusResponse();
    QJsonObject prepareMessagesResponse();

    static constexpr int MAX_MISSED_PINGS = 3;

    // Member variables
    QTcpSocket* socket;
    DatabaseManager* dbManager;
    quint32 userId;
    QString state;  // Obecny stan sesji
    QString sessionConnectionName;
    bool isAuthenticated;
    QTimer statusUpdateTimer;
    QTimer messagesCheckTimer;
    QTimer pingTimer;
    qint64 lastPingTime;
    qint64 lastSentMessageId = 0;
    int missedPings;
    QHash<QString, QJsonObject> unconfirmedMessages;
    QByteArray buffer;
    QVector<ChatMessage> messages;
};

#endif // CLIENTSESSION_H
