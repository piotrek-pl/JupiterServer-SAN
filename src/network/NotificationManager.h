// NotificationManager.h
#ifndef NOTIFICATIONMANAGER_H
#define NOTIFICATIONMANAGER_H

#include <QObject>
#include <QWebSocket>
#include <QSharedPointer>
#include <QScopedPointer>

class NotificationManager : public QObject
{
    Q_OBJECT
public:
    static NotificationManager& getInstance();

    void connectToServer(const QString& url);
    void disconnectFromServer();
    bool isConnected() const;

signals:
    void userStatusChanged(const QString& userId, const QString& newStatus);
    void newMessageReceived(const QString& fromUser, const QString& message);
    void friendListChanged(const QString& userId, bool added);
    void connectionStatusChanged(bool connected);

private:
    explicit NotificationManager(QObject *parent = nullptr);
    ~NotificationManager() override;

    NotificationManager(const NotificationManager&) = delete;
    NotificationManager& operator=(const NotificationManager&) = delete;

    void handleMessage(const QString& message);
    void setupConnections();

    QScopedPointer<QWebSocket> webSocket;
    QString serverUrl;
    bool connected;

private slots:
    void onConnected();
    void onDisconnected();
    void onTextMessageReceived(const QString& message);
    void onErrorOccurred(QAbstractSocket::SocketError error);  // Poprawiona nazwa
};

#endif // NOTIFICATIONMANAGER_H
