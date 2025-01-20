#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QPair>
#include <QVector>

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

    // Operacje na wiadomościach
    bool storeMessage(quint32 senderId, quint32 receiverId, const QString& message);
    QVector<QPair<QString, QString>> getChatHistory(quint32 userId, quint32 friendId, int limit = 50);

    // Operacje na znajomych
    bool addFriend(quint32 userId, quint32 friendId);
    QVector<QPair<quint32, QString>> getFriendsList(quint32 userId);

private:
    // Metody pomocnicze
    bool createTablesIfNotExist();
    bool verifyPassword(const QString& password, const QString& hash);
    bool validateUsername(const QString& username);
    bool validatePassword(const QString& password);
    bool userExists(const QString& username);
    bool userExists(quint32 userId);
    QString generateSalt();
    QString hashPassword(const QString& password);
    bool createFriendsList(quint32 userId);

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
