#include "DatabaseManager.h"
#include "DatabaseQueries.h"
#include "server/Protocol.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSqlError>
#include <QDebug>
#include <QSettings>
#include <QFile>
#include <QDir>

DatabaseManager::DatabaseConfig DatabaseManager::DatabaseConfig::instance;
bool DatabaseManager::mainInitialized = false;

DatabaseManager::DatabaseManager(QObject *parent)
    : QObject(parent)
    , configFilePath("config/database.conf")
    , initialized(false)
{
}

DatabaseManager::DatabaseManager(const QString& configPath, QObject *parent)
    : QObject(parent)
    , configFilePath(configPath)
    , initialized(false)
{
}

DatabaseManager::~DatabaseManager()
{
    if (database.isOpen()) {
        database.close();
    }
}

void DatabaseManager::setConfigPath(const QString& path)
{
    if (!initialized) {
        configFilePath = path;
    } else {
        qWarning() << "Cannot change config path after initialization";
    }
}

bool DatabaseManager::init()
{
    qDebug() << "Metoda DatabaseManager::init() została wywołana";
    qDebug() << "Dostępne sterowniki SQL:" << QSqlDatabase::drivers();

    if (!QSqlDatabase::isDriverAvailable("QMYSQL")) {
        qCritical() << "Sterownik QMYSQL nie jest dostępny!";
        return false;
    }

    // Wczytaj konfigurację
    QSettings settings(configFilePath, QSettings::IniFormat);

    // Zapisz konfigurację w statycznej instancji bez wartości domyślnych
    DatabaseConfig::instance.hostname = settings.value("Database/hostname").toString();
    DatabaseConfig::instance.database = settings.value("Database/database").toString();
    DatabaseConfig::instance.username = settings.value("Database/username").toString();
    DatabaseConfig::instance.password = settings.value("Database/password").toString();
    DatabaseConfig::instance.port = settings.value("Database/port").toInt();

    // Sprawdź czy wszystkie wymagane wartości są ustawione
    if (DatabaseConfig::instance.hostname.isEmpty() ||
        DatabaseConfig::instance.database.isEmpty() ||
        DatabaseConfig::instance.username.isEmpty() ||
        DatabaseConfig::instance.port == 0) {
        qCritical() << "Brakujące wartości w pliku konfiguracyjnym!";
        qCritical() << "Wymagane pola: hostname, database, username, port";
        return false;
    }

    // Utwórz główne połączenie
    database = QSqlDatabase::addDatabase("QMYSQL", "MainConnection");
    database.setHostName(DatabaseConfig::instance.hostname);
    database.setDatabaseName(DatabaseConfig::instance.database);
    database.setUserName(DatabaseConfig::instance.username);
    database.setPassword(DatabaseConfig::instance.password);
    database.setPort(DatabaseConfig::instance.port);

    if (!database.open()) {
        qWarning() << "Failed to open database:" << database.lastError().text();
        return false;
    }

    if (!createTablesIfNotExist()) {
        return false;
    }

    initialized = true;
    mainInitialized = true;
    qDebug() << "Baza danych została pomyślnie zainicjalizowana";
    return true;
}

bool DatabaseManager::createTablesIfNotExist()
{
    if (!database.transaction()) {
        qCritical() << "Failed to start transaction for creating tables";
        return false;
    }

    try {
        QSqlQuery query(database);

        if (!query.exec(DatabaseQueries::Create::USERS_TABLE)) {
            throw std::runtime_error("Failed to create users table: " + query.lastError().text().toStdString());
        }

        if (!query.exec(DatabaseQueries::Create::SESSIONS_TABLE)) {
            throw std::runtime_error("Failed to create sessions table: " + query.lastError().text().toStdString());
        }

        if (!database.commit()) {
            throw std::runtime_error("Failed to commit table creation");
        }

        qInfo() << "Database tables created successfully";
        return true;
    }
    catch (const std::exception& e) {
        qCritical() << "Error creating tables:" << e.what();
        database.rollback();
        return false;
    }
}

bool DatabaseManager::authenticateUser(const QString& username, const QString& password, quint32& userId)
{
    qDebug() << "=== Starting authentication for user:" << username << "===";

    // Sprawdź stan bazy danych
    if (!database.isOpen()) {
        qWarning() << "Database is not open during authentication!";
        if (!database.open()) {
            qWarning() << "Failed to reopen database:" << database.lastError().text();
            return false;
        }
    }

    if (!database.transaction()) {
        qWarning() << "Failed to start transaction for user authentication";
        qWarning() << "Transaction error:" << database.lastError().text();
        return false;
    }

    try {
        QSqlQuery query(database);
        query.prepare(DatabaseQueries::Users::AUTHENTICATE);
        query.addBindValue(username);

        qDebug() << "Executing authentication query for username:" << username;

        if (!query.exec()) {
            qWarning() << "Query execution failed:" << query.lastError().text();
            throw std::runtime_error("Authentication query failed");
        }

        if (!query.next()) {
            qWarning() << "No user found with username:" << username;
            throw std::runtime_error("User not found");
        }

        userId = query.value(0).toUInt();
        QString storedHash = query.value(1).toString();
        QString salt = query.value(2).toString();

        qDebug() << "Authentication details:";
        qDebug() << "- User ID:" << userId;
        qDebug() << "- Stored hash:" << storedHash;
        qDebug() << "- Salt:" << salt;
        qDebug() << "- Input password length:" << password.length();

        QString saltedPassword = password + salt;
        QString computedHash = hashPassword(saltedPassword);

        qDebug() << "Password verification:";
        qDebug() << "- Salted password:" << saltedPassword;
        qDebug() << "- Computed hash:" << computedHash;
        qDebug() << "- Stored hash:" << storedHash;
        qDebug() << "- Hashes match:" << (computedHash == storedHash);

        if (!verifyPassword(saltedPassword, storedHash)) {
            throw std::runtime_error("Invalid password");
        }

        if (!updateUserStatus(userId, "online")) {
            throw std::runtime_error("Failed to update user status");
        }

        if (!database.commit()) {
            throw std::runtime_error("Failed to commit transaction");
        }

        return true;
    }
    catch (const std::exception& e) {
        qWarning() << "Authentication error:" << e.what();
        database.rollback();
        return false;
    }
}

bool DatabaseManager::registerUser(const QString& username, const QString& password, const QString& email)
{
    if (!validateUsername(username) || !validatePassword(password)) {
        return false;
    }

    if (!database.transaction()) {
        qWarning() << "Failed to start transaction for user registration";
        return false;
    }

    try {
        if (userExists(username)) {
            throw std::runtime_error("Username already exists");
        }

        QString salt = generateSalt();
        QString hashedPassword = hashPassword(password + salt);

        QSqlQuery query(database);
        query.prepare("INSERT INTO users (username, password, salt, email, status) VALUES (?, ?, ?, ?, 'offline')");
        query.addBindValue(username);
        query.addBindValue(hashedPassword);
        query.addBindValue(salt);
        query.addBindValue(email);

        if (!query.exec()) {
            throw std::runtime_error("Failed to register user: " + query.lastError().text().toStdString());
        }

        if (!database.commit()) {
            throw std::runtime_error("Failed to commit registration");
        }

        return true;
    }
    catch (const std::exception& e) {
        qWarning() << "Registration error:" << e.what();
        database.rollback();
        return false;
    }
}

bool DatabaseManager::getUserStatus(quint32 userId, QString& status)
{
    if (!database.isOpen()) {
        qWarning() << "Database is not open";
        return false;
    }

    try {
        QSqlQuery query(database);
        query.prepare("SELECT status FROM users WHERE id = ?");
        query.addBindValue(userId);

        if (!query.exec()) {
            qWarning() << "Failed to execute status query:" << query.lastError().text();
            return false;
        }

        if (!query.next()) {
            qWarning() << "No status found for user ID:" << userId;
            status = Protocol::UserStatus::OFFLINE; // Domyślny status
            return true;
        }

        status = query.value(0).toString().toLower(); // Konwertuj na małe litery dla spójności

        // Walidacja statusu
        if (status != Protocol::UserStatus::ONLINE &&
            status != Protocol::UserStatus::OFFLINE &&
            status != Protocol::UserStatus::AWAY &&
            status != Protocol::UserStatus::BUSY) {
            qWarning() << "Invalid status in database:" << status << "for user ID:" << userId;
            status = Protocol::UserStatus::OFFLINE;
        }

        return true;
    }
    catch (const std::exception& e) {
        qWarning() << "Error getting user status:" << e.what();
        status = Protocol::UserStatus::OFFLINE;
        return false;
    }
}

bool DatabaseManager::updateUserStatus(quint32 userId, const QString& status)
{
    // Walidacja statusu przed rozpoczęciem transakcji
    QString normalizedStatus = status.toLower(); // Konwertuj na małe litery
    if (normalizedStatus != Protocol::UserStatus::ONLINE &&
        normalizedStatus != Protocol::UserStatus::OFFLINE &&
        normalizedStatus != Protocol::UserStatus::AWAY &&
        normalizedStatus != Protocol::UserStatus::BUSY) {
        qWarning() << "Invalid status value:" << status;
        return false;
    }

    if (!database.transaction()) {
        qWarning() << "Failed to start transaction for status update";
        return false;
    }

    try {
        QSqlQuery query(database);
        query.prepare(DatabaseQueries::Users::UPDATE_STATUS);
        query.addBindValue(normalizedStatus); // Używamy znormalizowanego statusu
        query.addBindValue(userId);

        if (!query.exec()) {
            throw std::runtime_error("Failed to update user status: " + query.lastError().text().toStdString());
        }

        if (!database.commit()) {
            throw std::runtime_error("Failed to commit status update");
        }

        qDebug() << "Successfully updated status for user" << userId << "to:" << normalizedStatus;
        return true;
    }
    catch (const std::exception& e) {
        qWarning() << "Status update error:" << e.what();
        database.rollback();
        return false;
    }
}

bool DatabaseManager::storeMessage(quint32 senderId, quint32 receiverId, const QString& message)
{
    if (!createChatTableIfNotExists(senderId, receiverId)) {
        return false;
    }

    if (!database.transaction()) {
        qWarning() << "Failed to start transaction for storing message";
        return false;
    }

    try {
        QString tableName = getChatTableName(senderId, receiverId);
        QSqlQuery query(database);
        query.prepare(DatabaseQueries::Messages::STORE_IN_CHAT.arg(tableName));
        query.addBindValue(senderId);
        query.addBindValue(message);

        if (!query.exec()) {
            throw std::runtime_error("Failed to store message: " + query.lastError().text().toStdString());
        }

        if (!database.commit()) {
            throw std::runtime_error("Failed to commit message storage");
        }

        return true;
    }
    catch (const std::exception& e) {
        qWarning() << "Message storage error:" << e.what();
        database.rollback();
        return false;
    }
}

bool DatabaseManager::addFriend(quint32 userId, quint32 friendId)
{
    if (!database.transaction()) {
        qWarning() << "Failed to start transaction for adding friend";
        return false;
    }

    try {
        if (!userExists(userId) || !userExists(friendId)) {
            throw std::runtime_error("Invalid user or friend ID");
        }

        if (!createFriendsList(userId)) {
            throw std::runtime_error("Failed to create friends list");
        }

        QSqlQuery query(database);
        query.prepare(DatabaseQueries::Friends::ADD.arg(userId));
        query.addBindValue(friendId);

        if (!query.exec()) {
            throw std::runtime_error("Failed to add friend: " + query.lastError().text().toStdString());
        }

        if (!database.commit()) {
            throw std::runtime_error("Failed to commit adding friend");
        }

        return true;
    }
    catch (const std::exception& e) {
        qWarning() << "Add friend error:" << e.what();
        database.rollback();
        return false;
    }
}

QVector<QPair<quint32, QString>> DatabaseManager::getFriendsList(quint32 userId)
{
    QVector<QPair<quint32, QString>> friendsList;

    qDebug() << "Getting friends list for user:" << userId;  // Debug log

    if (!database.transaction()) {
        qWarning() << "Failed to start transaction for getting friends list";
        return friendsList;
    }

    try {
        QString queryStr = DatabaseQueries::Friends::LIST.arg(QString::number(userId));
        QSqlQuery query(database);

        qDebug() << "Executing query:" << queryStr;  // Debug log

        if (!query.exec(queryStr)) {
            qWarning() << "Query error:" << query.lastError().text();  // Debug log
            throw std::runtime_error("Failed to get friends list: " + query.lastError().text().toStdString());
        }

        while (query.next()) {
            quint32 friendId = query.value(0).toUInt();
            QString username = query.value(1).toString();
            QString status = query.value(2).toString();  // Debug - dodaj status
            qDebug() << "Found friend:" << friendId << username << status;  // Debug log
            friendsList.append({friendId, username});
        }

        if (!database.commit()) {
            qWarning() << "Commit error:" << database.lastError().text();  // Debug log
            throw std::runtime_error("Failed to commit friends list query");
        }

        qDebug() << "Successfully found" << friendsList.size() << "friends";  // Debug log
        return friendsList;
    }
    catch (const std::exception& e) {
        qWarning() << "Error getting friends list:" << e.what();
        database.rollback();
        return friendsList;
    }
}

QVector<ChatMessage> DatabaseManager::getChatHistory(quint32 userId1, quint32 userId2,
                                                     int offset, int limit)
{
    QVector<ChatMessage> history;

    if (!database.isOpen()) {
        qWarning() << "Database is not open!";
        return history;
    }

    QString tableName = getChatTableName(userId1, userId2);
    if (!chatTableExists(tableName)) {
        return history;
    }

    QString queryStr = QString(DatabaseQueries::Messages::GET_CHAT_HISTORY).arg(tableName);
    QSqlQuery query(database);
    query.prepare(queryStr);
    query.bindValue(0, limit);
    query.bindValue(1, offset);

    if (!query.exec()) {
        qWarning() << "Failed to get chat history:" << query.lastError().text();
        return history;
    }

    while (query.next()) {
        ChatMessage msg;
        msg.username = query.value("username").toString();
        msg.message = query.value("message").toString();
        msg.timestamp = query.value("sent_at").toDateTime();
        msg.isRead = !query.value("read_at").isNull();
        history.append(msg);
    }

    return history;
}

bool DatabaseManager::hasMoreHistory(quint32 userId1, quint32 userId2, int offset)
{
    QString tableName = getChatTableName(userId1, userId2);
    if (!chatTableExists(tableName)) {
        return false;
    }

    QString queryStr = QString(DatabaseQueries::Messages::GET_MESSAGES_COUNT).arg(tableName);
    QSqlQuery query(database);

    if (!query.exec(queryStr)) {
        qWarning() << "Failed to get messages count:" << query.lastError().text();
        return false;
    }

    if (query.next()) {
        return query.value(0).toInt() > offset + Protocol::ChatHistory::MESSAGE_BATCH_SIZE;
    }

    return false;
}

QVector<UserSearchResult> DatabaseManager::searchUsers(const QString& query, quint32 currentUserId)
{
    QVector<UserSearchResult> results;

    qDebug() << "Searching for users with query:" << query
             << "excluding current user ID:" << currentUserId;

    if (!database.isOpen()) {
        qWarning() << "Database is not open during user search!";
        return results;
    }

    if (query.isEmpty()) {
        qWarning() << "Empty search query provided";
        return results;
    }

    try {
        QSqlQuery sqlQuery(database);
        // Przygotuj zapytanie SQL
        sqlQuery.prepare(
            "SELECT id, username FROM users "
            "WHERE username LIKE ? "
            "AND id != ? "  // Wykluczamy bieżącego użytkownika
            "ORDER BY username "
            "LIMIT 20"
            );

        // Dodaj parametry
        sqlQuery.addBindValue("%" + query + "%");  // % dla LIKE
        sqlQuery.addBindValue(currentUserId);

        // Wykonaj zapytanie
        if (!sqlQuery.exec()) {
            qWarning() << "Search users query failed:"
                       << sqlQuery.lastError().text();
            return results;
        }

        // Przetwórz wyniki
        while (sqlQuery.next()) {
            UserSearchResult result;
            result.id = sqlQuery.value(0).toUInt();
            result.username = sqlQuery.value(1).toString();

            qDebug() << "Found user:" << result.username
                     << "with ID:" << result.id;

            results.append(result);
        }

        qDebug() << "Search completed. Found" << results.size() << "users";
        return results;
    }
    catch (const std::exception& e) {
        qWarning() << "Error during user search:" << e.what();
        return results;
    }
}

bool DatabaseManager::verifyPassword(const QString& saltedPassword, const QString& hash)
{
    QString computedHash = hashPassword(saltedPassword);
    qDebug() << "Comparing hashes:" << computedHash << "vs" << hash;
    return computedHash == hash;
}

bool DatabaseManager::validateUsername(const QString& username)
{
    if (username.length() < Protocol::Validation::MIN_USERNAME_LENGTH || username.length() > Protocol::Validation::MAX_USERNAME_LENGTH) {
        return false;
    }

    QRegularExpression rx("^[a-zA-Z0-9_-]+$");
    return rx.match(username).hasMatch();
}

bool DatabaseManager::validatePassword(const QString& password)
{
    return password.length() >= Protocol::Validation::MIN_PASSWORD_LENGTH &&
           password.length() <= Protocol::Validation::MAX_PASSWORD_LENGTH;
}

bool DatabaseManager::userExists(const QString& username)
{
    QSqlQuery query(database);
    query.prepare(DatabaseQueries::Users::EXISTS_BY_NAME);
    query.addBindValue(username);

    if (query.exec() && query.next()) {
        return query.value(0).toInt() > 0;
    }
    return false;
}

bool DatabaseManager::userExists(quint32 userId)
{
    QSqlQuery query(database);
    query.prepare(DatabaseQueries::Users::EXISTS_BY_ID);
    query.addBindValue(userId);

    if (query.exec() && query.next()) {
        return query.value(0).toInt() > 0;
    }
    return false;
}

QString DatabaseManager::generateSalt()
{
    const QString possibleCharacters("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");
    QString salt;
    for(int i = 0; i < SALT_LENGTH; ++i) {
        int index = QRandomGenerator::global()->bounded(possibleCharacters.length());
        salt.append(possibleCharacters.at(index));
    }
    return salt;
}

QString DatabaseManager::hashPassword(const QString& password)
{
    QByteArray hash = QCryptographicHash::hash(
        password.toUtf8(),
        QCryptographicHash::Sha256
        );
    return QString(hash.toHex());
}

bool DatabaseManager::createFriendsList(quint32 userId)
{
    if (!database.transaction()) {
        qWarning() << "Failed to start transaction for creating friends list";
        return false;
    }

    try {
        QString createTableQuery = DatabaseQueries::Create::FRIENDS_TABLE.arg(userId);
        QSqlQuery query(database);

        if (!query.exec(createTableQuery)) {
            throw std::runtime_error("Failed to create friends table: " + query.lastError().text().toStdString());
        }

        if (!database.commit()) {
            throw std::runtime_error("Failed to commit friends table creation");
        }

        return true;
    }
    catch (const std::exception& e) {
        qWarning() << "Error creating friends list:" << e.what();
        database.rollback();
        return false;
    }
}

bool DatabaseManager::cloneConnectionForThread(const QString& connectionName)
{
    qDebug() << "Cloning database connection for session:" << connectionName;

    if (!mainInitialized) {  // Używamy statycznej flagi
        qWarning() << "Cannot clone connection - main database not initialized";
        return false;
    }

    if (QSqlDatabase::contains(connectionName)) {
        qDebug() << "Removing existing connection:" << connectionName;
        QSqlDatabase::removeDatabase(connectionName);
    }

    QSqlDatabase newDb = QSqlDatabase::addDatabase("QMYSQL", connectionName);

    // Użyj zapisanej konfiguracji
    newDb.setHostName(DatabaseConfig::instance.hostname);
    newDb.setDatabaseName(DatabaseConfig::instance.database);
    newDb.setUserName(DatabaseConfig::instance.username);
    newDb.setPassword(DatabaseConfig::instance.password);
    newDb.setPort(DatabaseConfig::instance.port);

    if (!newDb.open()) {
        qWarning() << "Failed to open session database connection:"
                   << connectionName << "-" << newDb.lastError().text();
        return false;
    }

    qDebug() << "Successfully created new database connection:" << connectionName;

    // Ustaw to połączenie jako aktywne dla tej instancji
    database = newDb;
    initialized = true;

    return true;
}

// Metoda pomocnicza do generowania nazwy tabeli chatu
QString DatabaseManager::getChatTableName(quint32 userId1, quint32 userId2)
{
    // Zawsze używamy mniejszego ID jako pierwsze
    quint32 smallerId = qMin(userId1, userId2);
    quint32 largerId = qMax(userId1, userId2);
    return QString(DatabaseQueries::Tables::CHAT_PREFIX).arg(smallerId).arg(largerId);
}

// Sprawdzenie czy tabela chatu istnieje
bool DatabaseManager::chatTableExists(const QString& tableName)
{
    QSqlQuery query(database);
    query.prepare(DatabaseQueries::Messages::CHECK_CHAT_TABLE_EXISTS);
    query.addBindValue(tableName);

    if (!query.exec() || !query.next()) {
        qWarning() << "Failed to check if chat table exists:" << query.lastError().text();
        return false;
    }

    return query.value(0).toInt() > 0;
}

// Tworzenie indeksów dla tabeli chatu
void DatabaseManager::createChatIndexes(const QString& tableName)
{
    QSqlQuery query(database);
    QString indexQuery = DatabaseQueries::Create::CHAT_INDEXES.arg(tableName);

    if (!query.exec(indexQuery)) {
        qWarning() << "Failed to create indexes for chat table:" << query.lastError().text();
    }
}

// Tworzenie nowej tabeli chatu jeśli nie istnieje
bool DatabaseManager::createChatTableIfNotExists(quint32 userId1, quint32 userId2)
{
    QString tableName = getChatTableName(userId1, userId2);

    if (chatTableExists(tableName)) {
        return true;
    }

    if (!database.transaction()) {
        qWarning() << "Failed to start transaction for creating chat table";
        return false;
    }

    try {
        QSqlQuery query(database);
        QString createQuery = DatabaseQueries::Create::CHAT_TABLE.arg(tableName);

        if (!query.exec(createQuery)) {
            throw std::runtime_error("Failed to create chat table: " + query.lastError().text().toStdString());
        }

        createChatIndexes(tableName);

        if (!database.commit()) {
            throw std::runtime_error("Failed to commit chat table creation");
        }

        qInfo() << "Created new chat table:" << tableName;
        return true;
    }
    catch (const std::exception& e) {
        qWarning() << "Error creating chat table:" << e.what();
        database.rollback();
        return false;
    }
}

// Oznaczanie wiadomości jako przeczytanych
bool DatabaseManager::markChatAsRead(quint32 userId, quint32 friendId)
{
    QString tableName = getChatTableName(userId, friendId);

    if (!chatTableExists(tableName)) {
        return true; // Brak tabeli oznacza brak nieprzeczytanych wiadomości
    }

    if (!database.transaction()) {
        qWarning() << "Failed to start transaction for marking messages as read";
        return false;
    }

    try {
        QSqlQuery query(database);
        query.prepare(DatabaseQueries::Messages::MARK_CHAT_READ.arg(tableName));
        query.addBindValue(userId);

        if (!query.exec()) {
            throw std::runtime_error("Failed to mark messages as read: " + query.lastError().text().toStdString());
        }

        if (!database.commit()) {
            throw std::runtime_error("Failed to commit marking messages as read");
        }

        return true;
    }
    catch (const std::exception& e) {
        qWarning() << "Error marking messages as read:" << e.what();
        database.rollback();
        return false;
    }
}

QVector<ChatMessage> DatabaseManager::getLatestMessages(quint32 userId1, quint32 userId2,
                                                        int limit)
{
    QVector<ChatMessage> history;

    if (!database.isOpen()) {
        qWarning() << "Database is not open!";
        return history;
    }

    QString tableName = getChatTableName(userId1, userId2);
    if (!chatTableExists(tableName)) {
        return history;
    }

    QString queryStr = QString(DatabaseQueries::Messages::GET_LATEST_MESSAGES)
                           .arg(tableName)
                           .arg(tableName)
                           .arg(tableName);
    QSqlQuery query(database);
    query.prepare(queryStr);
    query.bindValue(0, limit);

    if (!query.exec()) {
        qWarning() << "Failed to get latest messages:" << query.lastError().text();
        return history;
    }

    while (query.next()) {
        ChatMessage msg;
        msg.username = query.value("username").toString();
        msg.message = query.value("message").toString();
        msg.timestamp = query.value("sent_at").toDateTime();
        msg.isRead = !query.value("read_at").isNull();
        history.append(msg);
    }

    return history;
}

QVector<quint32> DatabaseManager::getUnreadMessagesUsers(quint32 userId)
{
    QVector<quint32> usersWithUnread;
    QVector<QPair<quint32, QString>> friendsList = getFriendsList(userId);

    for (const auto& friend_ : friendsList) {
        quint32 friendId = friend_.first;
        QString tableName = getChatTableName(userId, friendId);

        if (!chatTableExists(tableName)) {
            continue;
        }

        QSqlQuery query(database);
        query.prepare(DatabaseQueries::Messages::GET_UNREAD_COUNT.arg(tableName));
        query.addBindValue(userId);

        if (query.exec() && query.next()) {
            int unreadCount = query.value(0).toInt();
            if (unreadCount > 0) {
                usersWithUnread.append(friendId);
            }
        }
    }

    return usersWithUnread;
}

bool DatabaseManager::removeFriend(quint32 userId, quint32 friendId)
{
    qDebug() << "Attempting to remove friend" << friendId << "for user" << userId;

    if (!database.isOpen()) {
        qWarning() << "Database is not open during friend removal!";
        return false;
    }

    if (!database.transaction()) {
        qWarning() << "Failed to start transaction for friend removal";
        return false;
    }

    try {
        // Sprawdź czy użytkownicy istnieją
        if (!userExists(userId) || !userExists(friendId)) {
            throw std::runtime_error("Invalid user or friend ID");
        }

        // Usuń znajomego z listy użytkownika
        QSqlQuery query(database);
        query.prepare(DatabaseQueries::Friends::REMOVE.arg(userId));
        query.addBindValue(friendId);

        if (!query.exec()) {
            throw std::runtime_error("Failed to remove friend from user's list: " +
                                     query.lastError().text().toStdString());
        }

        // Usuń użytkownika z listy znajomego
        query.prepare(DatabaseQueries::Friends::REMOVE.arg(friendId));
        query.addBindValue(userId);

        if (!query.exec()) {
            throw std::runtime_error("Failed to remove user from friend's list: " +
                                     query.lastError().text().toStdString());
        }

        if (!database.commit()) {
            throw std::runtime_error("Failed to commit friend removal");
        }

        qDebug() << "Successfully removed friend relationship between" << userId << "and" << friendId;
        return true;
    }
    catch (const std::exception& e) {
        qWarning() << "Error removing friend:" << e.what();
        database.rollback();
        return false;
    }
}

bool DatabaseManager::createInvitationTables(quint32 userId)
{
    if (!database.isOpen()) {
        qWarning() << "Database is not open while creating invitation tables";
        return false;
    }

    if (!database.transaction()) {
        qWarning() << "Failed to start transaction for creating invitation tables";
        return false;
    }

    try {
        QSqlQuery query(database);

        // Tworzenie tabeli wysłanych zaproszeń
        QString sentTableQuery = DatabaseQueries::Create::SENT_INVITATIONS_TABLE.arg(userId);
        if (!query.exec(sentTableQuery)) {
            throw std::runtime_error("Failed to create sent invitations table: " +
                                     query.lastError().text().toStdString());
        }

        // Tworzenie tabeli otrzymanych zaproszeń
        QString receivedTableQuery = DatabaseQueries::Create::RECEIVED_INVITATIONS_TABLE.arg(userId);
        if (!query.exec(receivedTableQuery)) {
            throw std::runtime_error("Failed to create received invitations table: " +
                                     query.lastError().text().toStdString());
        }

        if (!database.commit()) {
            throw std::runtime_error("Failed to commit invitation tables creation");
        }

        qDebug() << "Successfully created invitation tables for user" << userId;
        return true;
    }
    catch (const std::exception& e) {
        qWarning() << "Error creating invitation tables:" << e.what();
        database.rollback();
        return false;
    }
}

bool DatabaseManager::sendFriendInvitation(quint32 fromUserId, quint32 toUserId)
{
    if (!database.isOpen()) {
        qWarning() << "Database is not open while sending invitation";
        return false;
    }

    // Sprawdź czy nie są już znajomymi
    QVector<QPair<quint32, QString>> friendsList = getFriendsList(fromUserId);
    for (const auto& friend_ : friendsList) {
        if (friend_.first == toUserId) {
            qWarning() << "Users are already friends";
            return false;
        }
    }

    // Sprawdź czy nie ma już oczekującego zaproszenia
    if (checkPendingInvitation(fromUserId, toUserId)) {
        qWarning() << "Pending invitation already exists";
        return false;
    }

    if (!database.transaction()) {
        qWarning() << "Failed to start transaction for sending invitation";
        return false;
    }

    try {
        QSqlQuery query(database);

        // Pobierz nazwę użytkownika docelowego
        query.prepare(DatabaseQueries::Users::GET_USERNAME);
        query.addBindValue(toUserId);
        if (!query.exec() || !query.next()) {
            throw std::runtime_error("Failed to get target username");
        }
        QString toUsername = query.value(0).toString();

        // Dodaj wpis do tabeli wysłanych zaproszeń
        query.prepare(DatabaseQueries::Invitations::ADD_SENT.arg(fromUserId));
        query.addBindValue(toUserId);
        query.addBindValue(toUsername);
        if (!query.exec()) {
            throw std::runtime_error("Failed to add sent invitation");
        }

        // Pobierz nazwę użytkownika wysyłającego
        query.prepare(DatabaseQueries::Users::GET_USERNAME);
        query.addBindValue(fromUserId);
        if (!query.exec() || !query.next()) {
            throw std::runtime_error("Failed to get sender username");
        }
        QString fromUsername = query.value(0).toString();

        // Dodaj wpis do tabeli otrzymanych zaproszeń
        query.prepare(DatabaseQueries::Invitations::ADD_RECEIVED.arg(toUserId));
        query.addBindValue(fromUserId);
        query.addBindValue(fromUsername);
        if (!query.exec()) {
            throw std::runtime_error("Failed to add received invitation");
        }

        if (!database.commit()) {
            throw std::runtime_error("Failed to commit sending invitation");
        }

        qDebug() << "Successfully sent invitation from" << fromUserId << "to" << toUserId;
        return true;
    }
    catch (const std::exception& e) {
        qWarning() << "Error sending invitation:" << e.what();
        database.rollback();
        return false;
    }
}

bool DatabaseManager::acceptFriendInvitation(quint32 userId, int requestId)
{
    if (!database.isOpen()) {
        qWarning() << "Database is not open while accepting invitation";
        return false;
    }

    if (!database.transaction()) {
        qWarning() << "Failed to start transaction for accepting invitation";
        return false;
    }

    try {
        QSqlQuery query(database);

        // 1. Pobierz informacje o otrzymanym zaproszeniu bez sprawdzania statusu
        query.prepare(QString("SELECT from_user_id, created_at, status FROM user_%1_received_invitations "
                              "WHERE request_id = ?")
                          .arg(userId));
        query.bindValue(0, requestId);

        if (!query.exec() || !query.next()) {
            qWarning() << "Invitation not found";
            throw std::runtime_error("Invitation not found");
        }

        QString currentStatus = query.value("status").toString();
        if (currentStatus != "pending") {
            qWarning() << "Cannot accept invitation with status:" << currentStatus;
            throw std::runtime_error("Invitation is not in pending state");
        }

        quint32 fromUserId = query.value("from_user_id").toUInt();
        QDateTime createdAt = query.value("created_at").toDateTime();

        // 2. Zaktualizuj status w tabeli otrzymanych zaproszeń
        query.prepare(QString("UPDATE user_%1_received_invitations "
                              "SET status = ? "
                              "WHERE request_id = ?")
                          .arg(userId));
        query.bindValue(0, Protocol::InvitationStatus::ACCEPTED);
        query.bindValue(1, requestId);

        if (!query.exec() || query.numRowsAffected() == 0) {
            qWarning() << "Failed to update received invitation:" << query.lastError().text();
            throw std::runtime_error("Failed to update received invitation");
        }

        // 3. Znajdź i zaktualizuj odpowiednie zaproszenie w tabeli wysłanych
        query.prepare(QString("UPDATE user_%1_sent_invitations "
                              "SET status = ? "
                              "WHERE to_user_id = ? "
                              "AND created_at = ?")
                          .arg(fromUserId));
        query.bindValue(0, Protocol::InvitationStatus::ACCEPTED);
        query.bindValue(1, userId);
        query.bindValue(2, createdAt);

        if (!query.exec() || query.numRowsAffected() == 0) {
            qWarning() << "Failed to update sent invitation:" << query.lastError().text();
            throw std::runtime_error("Failed to update sent invitation");
        }

        // 4. Dodaj relację znajomych w obie strony
        if (!addFriend(userId, fromUserId)) {
            qWarning() << "Failed to add friend relationship (user->friend)";
            throw std::runtime_error("Failed to create friend relationship (user->friend)");
        }
        if (!addFriend(fromUserId, userId)) {
            qWarning() << "Failed to add friend relationship (friend->user)";
            throw std::runtime_error("Failed to create friend relationship (friend->user)");
        }

        if (!database.commit()) {
            throw std::runtime_error("Failed to commit accepting invitation");
        }

        qDebug() << "Successfully accepted invitation" << requestId
                 << "from user" << fromUserId
                 << "to user" << userId;
        return true;
    }
    catch (const std::exception& e) {
        qWarning() << "Error accepting invitation:" << e.what();
        database.rollback();
        return false;
    }
}

bool DatabaseManager::rejectFriendInvitation(quint32 userId, int requestId)
{
    if (!database.isOpen()) {
        qWarning() << "Database is not open while rejecting invitation";
        return false;
    }

    if (!database.transaction()) {
        qWarning() << "Failed to start transaction for rejecting invitation";
        return false;
    }

    try {
        QSqlQuery query(database);

        // 1. Pobierz informacje o otrzymanym zaproszeniu
        query.prepare(DatabaseQueries::Invitations::GET_RECEIVED_INVITATION_DETAILS.arg(userId));
        query.bindValue(0, requestId);

        if (!query.exec() || !query.next()) {
            throw std::runtime_error("Received invitation not found or not pending");
        }

        quint32 fromUserId = query.value(0).toUInt();
        QDateTime createdAt = query.value(1).toDateTime();

        // 2. Zaktualizuj status w tabeli otrzymanych zaproszeń
        query.prepare(DatabaseQueries::Invitations::UPDATE_RECEIVED_INVITATION_STATUS_REJECTED.arg(userId));
        query.bindValue(0, requestId);

        if (!query.exec() || query.numRowsAffected() == 0) {
            throw std::runtime_error("Failed to update received invitation");
        }

        // 3. Znajdź i zaktualizuj odpowiednie zaproszenie w tabeli wysłanych
        query.prepare(DatabaseQueries::Invitations::UPDATE_INVITATION_STATUS_REJECTED.arg(fromUserId));
        query.bindValue(0, userId);
        query.bindValue(1, createdAt);

        if (!query.exec() || query.numRowsAffected() == 0) {
            throw std::runtime_error("Failed to update sent invitation");
        }

        if (!database.commit()) {
            throw std::runtime_error("Failed to commit rejecting invitation");
        }

        qDebug() << "Successfully rejected invitation" << requestId
                 << "from user" << fromUserId
                 << "to user" << userId;
        return true;
    }
    catch (const std::exception& e) {
        qWarning() << "Error rejecting invitation:" << e.what();
        database.rollback();
        return false;
    }
}

bool DatabaseManager::cancelFriendInvitation(quint32 userId, int requestId)
{
    if (!database.isOpen()) {
        qWarning() << "Database is not open while cancelling invitation";
        return false;
    }

    if (!database.transaction()) {
        qWarning() << "Failed to start transaction for cancelling invitation";
        return false;
    }

    try {
        QSqlQuery query(database);

        // Pobierz dane zaproszenia
        query.prepare(DatabaseQueries::Invitations::GET_SENT.arg(userId));
        query.addBindValue(requestId);
        if (!query.exec() || !query.next()) {
            throw std::runtime_error("Failed to get invitation details");
        }

        quint32 toUserId = query.value("to_user_id").toUInt();

        // Aktualizuj status w obu tabelach
        if (!updateBothInvitationStatuses(userId, toUserId, requestId, Protocol::InvitationStatus::CANCELLED)) {
            throw std::runtime_error("Failed to update invitation statuses");
        }

        if (!database.commit()) {
            throw std::runtime_error("Failed to commit cancelling invitation");
        }

        qDebug() << "Successfully cancelled invitation" << requestId << "for user" << userId;
        return true;
    }
    catch (const std::exception& e) {
        qWarning() << "Error cancelling invitation:" << e.what();
        database.rollback();
        return false;
    }
}

bool DatabaseManager::checkPendingInvitation(quint32 fromUserId, quint32 toUserId)
{
    if (!database.isOpen()) {
        qWarning() << "Database is not open while checking pending invitation";
        return false;
    }

    try {
        QSqlQuery query(database);
        query.prepare(DatabaseQueries::Invitations::CHECK_PENDING.arg(fromUserId));
        query.addBindValue(toUserId);

        if (!query.exec()) {
            throw std::runtime_error("Failed to check pending invitation: " +
                                     query.lastError().text().toStdString());
        }

        if (query.next()) {
            return query.value(0).toInt() > 0;
        }

        return false;
    }
    catch (const std::exception& e) {
        qWarning() << "Error checking pending invitation:" << e.what();
        return false;
    }
}

bool DatabaseManager::updateInvitationStatus(quint32 userId, int requestId,
                                             const QString& status, bool isSender)
{
    if (!database.isOpen()) {
        qWarning() << "Database is not open while updating invitation status";
        return false;
    }

    try {
        QSqlQuery query(database);
        QString updateQuery = isSender ?
                                  DatabaseQueries::Invitations::UPDATE_SENT_STATUS.arg(userId) :
                                  DatabaseQueries::Invitations::UPDATE_RECEIVED_STATUS.arg(userId);

        query.prepare(updateQuery);
        query.addBindValue(status);
        query.addBindValue(requestId);

        if (!query.exec()) {
            throw std::runtime_error("Failed to update invitation status: " +
                                     query.lastError().text().toStdString());
        }

        return query.numRowsAffected() > 0;
    }
    catch (const std::exception& e) {
        qWarning() << "Error updating invitation status:" << e.what();
        return false;
    }
}

bool DatabaseManager::updateBothInvitationStatuses(quint32 fromUserId, quint32 toUserId,
                                                   int requestId, const QString& status)
{
    if (!database.isOpen()) {
        qWarning() << "Database is not open while updating both invitation statuses";
        return false;
    }

    if (!database.transaction()) {
        qWarning() << "Failed to start transaction for updating invitation statuses";
        return false;
    }

    try {
        QSqlQuery query(database);

        // 1. Pobierz informacje o zaproszeniu z tabeli wysłanych
        query.prepare(DatabaseQueries::Invitations::GET_SENT_INVITATION_DETAILS.arg(fromUserId));
        query.bindValue(0, requestId);

        if (!query.exec() || !query.next()) {
            throw std::runtime_error("Sent invitation not found or not pending");
        }

        toUserId = query.value(0).toUInt();
        QDateTime createdAt = query.value(1).toDateTime();

        // 2. Aktualizuj status w tabeli wysłanych zaproszeń
        query.prepare(DatabaseQueries::Invitations::UPDATE_SENT_INVITATION_STATUS.arg(fromUserId));
        query.bindValue(0, status);
        query.bindValue(1, requestId);

        if (!query.exec() || query.numRowsAffected() == 0) {
            throw std::runtime_error("Failed to update sent invitation");
        }

        // 3. Znajdź i zaktualizuj odpowiednie zaproszenie w tabeli otrzymanych
        query.prepare(DatabaseQueries::Invitations::UPDATE_RECEIVED_INVITATION_STATUS_BY_TIMESTAMP.arg(toUserId));
        query.bindValue(0, status);
        query.bindValue(1, fromUserId);
        query.bindValue(2, createdAt);

        if (!query.exec() || query.numRowsAffected() == 0) {
            throw std::runtime_error("Failed to update received invitation");
        }

        if (!database.commit()) {
            throw std::runtime_error("Failed to commit invitation updates");
        }

        qDebug() << "Successfully updated invitation statuses from user" << fromUserId
                 << "to user" << toUserId << "with status:" << status;
        return true;
    }
    catch (const std::exception& e) {
        qWarning() << "Error updating invitation statuses:" << e.what();
        database.rollback();
        return false;
    }
}

QVector<FriendInvitation> DatabaseManager::getSentInvitations(quint32 userId)
{
    QVector<FriendInvitation> invitations;

    if (!database.isOpen()) {
        qWarning() << "Database is not open while getting sent invitations";
        return invitations;
    }

    try {
        QSqlQuery query(database);
        query.prepare(DatabaseQueries::Invitations::GET_SENT.arg(userId));

        if (!query.exec()) {
            throw std::runtime_error("Failed to get sent invitations: " +
                                     query.lastError().text().toStdString());
        }

        while (query.next()) {
            FriendInvitation invitation;
            invitation.requestId = query.value("request_id").toInt();
            invitation.userId = query.value("to_user_id").toUInt();
            invitation.username = query.value("to_username").toString();
            invitation.status = query.value("status").toString();
            invitation.timestamp = query.value("created_at").toDateTime();

            invitations.append(invitation);
        }

        qDebug() << "Successfully retrieved" << invitations.size()
                 << "sent invitations for user" << userId;
        return invitations;
    }
    catch (const std::exception& e) {
        qWarning() << "Error getting sent invitations:" << e.what();
        return QVector<FriendInvitation>();
    }
}

QVector<FriendInvitation> DatabaseManager::getReceivedInvitations(quint32 userId)
{
    QVector<FriendInvitation> invitations;

    if (!database.isOpen()) {
        qWarning() << "Database is not open while getting received invitations";
        return invitations;
    }

    try {
        QSqlQuery query(database);
        query.prepare(DatabaseQueries::Invitations::GET_RECEIVED.arg(userId));

        if (!query.exec()) {
            throw std::runtime_error("Failed to get received invitations: " +
                                     query.lastError().text().toStdString());
        }

        while (query.next()) {
            FriendInvitation invitation;
            invitation.requestId = query.value("request_id").toInt();
            invitation.userId = query.value("from_user_id").toUInt();
            invitation.username = query.value("from_username").toString();
            invitation.status = query.value("status").toString();
            invitation.timestamp = query.value("created_at").toDateTime();

            invitations.append(invitation);
        }

        qDebug() << "Successfully retrieved" << invitations.size()
                 << "received invitations for user" << userId;
        return invitations;
    }
    catch (const std::exception& e) {
        qWarning() << "Error getting received invitations:" << e.what();
        return QVector<FriendInvitation>();
    }
}

bool DatabaseManager::sendFriendRequest(int senderId, int targetUserId) {
    if (!database.isOpen()) {
        qWarning() << "Database is not open while sending friend request";
        return false;
    }

    qDebug() << "Processing friend request from user" << senderId << "to user" << targetUserId;

    if (!database.transaction()) {
        qWarning() << "Failed to start transaction for friend request";
        return false;
    }

    try {
        QSqlQuery query(database);

        // Sprawdź czy użytkownik istnieje
        query.prepare(DatabaseQueries::Invitations::CHECK_USER_EXISTS);
        query.bindValue(0, targetUserId);

        if (!query.exec() || !query.next()) {
            throw std::runtime_error("Failed to check if user exists: " +
                                     query.lastError().text().toStdString());
        }

        if (query.value(0).toInt() == 0) {
            throw std::runtime_error("Target user not found");
        }

        // Sprawdź czy nie są już znajomymi
        query.prepare(DatabaseQueries::Invitations::CHECK_IF_FRIENDS.arg(senderId));
        query.bindValue(0, targetUserId);

        if (!query.exec() || !query.next()) {
            throw std::runtime_error("Failed to check if users are friends: " +
                                     query.lastError().text().toStdString());
        }

        if (query.value(0).toInt() > 0) {
            throw std::runtime_error("Users are already friends");
        }

        // Sprawdź czy nie ma już oczekującego zaproszenia
        query.prepare(DatabaseQueries::Invitations::CHECK_PENDING_INVITATION.arg(senderId));
        query.bindValue(0, targetUserId);

        if (!query.exec() || !query.next()) {
            throw std::runtime_error("Failed to check pending invitations: " +
                                     query.lastError().text().toStdString());
        }

        if (query.value(0).toInt() > 0) {
            throw std::runtime_error("Friend request already sent");
        }

        // Dodaj zaproszenie do tabeli wysłanych zaproszeń
        query.prepare(DatabaseQueries::Invitations::ADD_FRIEND_REQUEST_SENT.arg(senderId));
        query.bindValue(0, targetUserId);

        if (!query.exec()) {
            throw std::runtime_error("Failed to add sent invitation: " +
                                     query.lastError().text().toStdString());
        }

        // Dodaj zaproszenie do tabeli otrzymanych zaproszeń
        query.prepare(DatabaseQueries::Invitations::ADD_FRIEND_REQUEST_RECEIVED.arg(targetUserId));
        query.bindValue(0, senderId);

        if (!query.exec()) {
            throw std::runtime_error("Failed to add received invitation: " +
                                     query.lastError().text().toStdString());
        }

        if (!database.commit()) {
            throw std::runtime_error("Failed to commit friend request transaction");
        }

        qDebug() << "Friend request sent successfully from user" << senderId << "to user" << targetUserId;
        return true;
    }
    catch (const std::exception& e) {
        qWarning() << "Error sending friend request:" << e.what();
        database.rollback();
        return false;
    }
}

QString DatabaseManager::getUserUsername(quint32 userId) {
    QSqlQuery query(database);
    query.prepare("SELECT username FROM users WHERE id = ?");
    query.addBindValue(userId);

    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }
    return QString();
}

quint32 DatabaseManager::getFriendRequestTargetUserId(quint32 userId, int requestId) {
    if (!database.isOpen()) {
        qWarning() << "Database is not open while getting target user ID";
        return 0;
    }

    try {
        QSqlQuery query(database);
        query.prepare(DatabaseQueries::Invitations::GET_SENT.arg(userId));
        query.addBindValue(requestId);

        if (!query.exec() || !query.next()) {
            qWarning() << "Failed to get invitation details for request ID:" << requestId;
            return 0;
        }

        return query.value("to_user_id").toUInt();
    }
    catch (const std::exception& e) {
        qWarning() << "Error getting target user ID:" << e.what();
        return 0;
    }
}
