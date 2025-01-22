#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QPair>
#include <QVector>
#include <QDateTime>
#include "server/Protocol.h"

// Struktura reprezentująca wiadomość w chacie
struct ChatMessage {
    QString username;
    QString message;
    QDateTime timestamp;
    bool isRead;
};

class DatabaseManager : public QObject
{
    Q_OBJECT

public:
    struct DatabaseConfig {
        QString hostname;
        QString database;
        QString username;
        QString password;
        int port;

        static DatabaseConfig instance;
    };

    explicit DatabaseManager(QObject *parent = nullptr);
    explicit DatabaseManager(const QString& configPath, QObject *parent = nullptr);
    ~DatabaseManager();

    bool init();
    const QString& configPath() const { return configFilePath; }
    void setConfigPath(const QString& path);
    QSqlDatabase& getDatabase() { return database; }
    bool isInitialized() const { return initialized; }
    bool cloneConnectionForThread(const QString& connectionName);

#ifdef QT_DEBUG
    bool reinitializeTables() { return createTablesIfNotExist(); }
#endif

    // Operacje na użytkownikach
    bool registerUser(const QString& username, const QString& password);
    bool authenticateUser(const QString& username, const QString& password, quint32& userId);
    bool getUserStatus(quint32 userId, QString& status);
    bool updateUserStatus(quint32 userId, const QString& status);

    // Operacje na wiadomościach - nowa implementacja chatów
    bool storeMessage(quint32 senderId, quint32 receiverId, const QString& message);
    QVector<ChatMessage> getChatHistory(quint32 userId1, quint32 userId2,
                                        int offset = 0,
                                        int limit = Protocol::ChatHistory::MESSAGE_BATCH_SIZE);
    bool hasMoreHistory(quint32 userId1, quint32 userId2, int offset);
    bool markChatAsRead(quint32 userId, quint32 friendId);

    // Operacje na znajomych
    bool addFriend(quint32 userId, quint32 friendId);
    QVector<QPair<quint32, QString>> getFriendsList(quint32 userId);
    QVector<ChatMessage> getLatestMessages(quint32 userId1, quint32 userId2,
                                           int limit = Protocol::ChatHistory::MESSAGE_BATCH_SIZE);
    QVector<QJsonObject> getNewMessages(quint32 userId, qint64 lastMessageId);

private:
    // Metody pomocnicze dla użytkowników
    bool createTablesIfNotExist();
    bool verifyPassword(const QString& password, const QString& hash);
    bool validateUsername(const QString& username);
    bool validatePassword(const QString& password);
    bool userExists(const QString& username);
    bool userExists(quint32 userId);
    QString generateSalt();
    QString hashPassword(const QString& password);
    bool createFriendsList(quint32 userId);

    // Nowe metody pomocnicze dla chatów
    QString getChatTableName(quint32 userId1, quint32 userId2);
    bool createChatTableIfNotExists(quint32 userId1, quint32 userId2);
    bool chatTableExists(const QString& tableName);
    void createChatIndexes(const QString& tableName);

    // Stałe
    static constexpr int MIN_USERNAME_LENGTH = 3;
    static constexpr int MAX_USERNAME_LENGTH = 32;
    static constexpr int MIN_PASSWORD_LENGTH = 8;
    static constexpr int MAX_PASSWORD_LENGTH = 64;
    static constexpr int SALT_LENGTH = 16;

    static bool mainInitialized;

    // Pola prywatne
    QString configFilePath;
    QSqlDatabase database;
    bool initialized;
    QString mainConnectionName;
};

#endif // DATABASEMANAGER_H
