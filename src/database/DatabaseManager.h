#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include "DatabaseQueries.h"
#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QString>
#include <QVector>
#include <QPair>
#include <QDebug>

class DatabaseManager : public QObject
{
    Q_OBJECT
public:
    explicit DatabaseManager(QObject *parent = nullptr);
    ~DatabaseManager();

    // Inicjalizacja i status
    bool init();
    bool isInitialized() const { return m_isInitialized; }
    QSqlDatabase& getDatabase() { return m_db; }

    // Metody autoryzacji
    bool authenticateUser(const QString& username, const QString& password, quint32& userId);
    bool registerUser(const QString& username, const QString& password);
    bool updateUserStatus(quint32 userId, const QString& status);
    bool getUserStatus(quint32 userId, QString& status);

    // Metody zarządzania znajomymi
    bool addFriend(quint32 userId, quint32 friendId);
    bool removeFriend(quint32 userId, quint32 friendId);
    bool createFriendsList(quint32 userId);
    QVector<QPair<quint32, QString>> getFriendsList(quint32 userId);
    bool isFriend(quint32 userId, quint32 friendId);

    // Metody zarządzania wiadomościami
    bool storeMessage(quint32 senderId, quint32 receiverId, const QString& message);
    QVector<QPair<QString, QString>> getChatHistory(quint32 userId, quint32 friendId, int limit = 50);
    bool markMessageAsRead(quint32 messageId);
    bool deleteMessage(quint32 messageId, quint32 userId);

    // Metody pomocnicze
    bool userExists(const QString& username);
    bool userExists(quint32 userId);
    QString getUserName(quint32 userId);

private:
    // Metody pomocnicze do inicjalizacji bazy danych
    bool createTablesIfNotExist();
    bool createUsersTable();
    bool createFriendsTable(quint32 userId);
    bool createMessagesTable();
    bool createUserSessionsTable();

    // Metody pomocnicze do wykonywania zapytań
    bool executeQuery(const QString& query, const QVector<QVariant>& bindings = QVector<QVariant>());
    bool executeQuery(QSqlQuery& query);

    // Metody pomocnicze do bezpieczeństwa
    QString hashPassword(const QString& password);
    bool verifyPassword(const QString& password, const QString& hash);
    QString generateSalt();

    // Metody pomocnicze do walidacji
    bool validateUsername(const QString& username);
    bool validatePassword(const QString& password);

private:
    QSqlDatabase m_db;
    bool m_isInitialized;

    // Stałe walidacyjne
    static constexpr int MIN_USERNAME_LENGTH = 3;
    static constexpr int MAX_USERNAME_LENGTH = 32;
    static constexpr int MIN_PASSWORD_LENGTH = 6;
    static constexpr int MAX_PASSWORD_LENGTH = 128;
    static constexpr int SALT_LENGTH = 16;

    // Stałe konfiguracyjne połączenia z bazą
    static const QString CONFIG_KEY_HOSTNAME;
    static const QString CONFIG_KEY_DATABASE;
    static const QString CONFIG_KEY_USERNAME;
    static const QString CONFIG_KEY_PASSWORD;
    static const QString CONFIG_KEY_PORT;

    static const QString DEFAULT_HOSTNAME;
    static const QString DEFAULT_DATABASE;
    static const QString DEFAULT_USERNAME;
    static const QString DEFAULT_PASSWORD;
    static constexpr int DEFAULT_PORT = 3306;
    static constexpr int MIN_PORT = 1;
    static constexpr int MAX_PORT = 65535;
};

#endif // DATABASEMANAGER_H
