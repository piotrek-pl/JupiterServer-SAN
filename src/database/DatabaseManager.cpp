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

    // Zapisz konfigurację w statycznej instancji
    DatabaseConfig::instance.hostname = settings.value("Database/hostname", "localhost").toString();
    DatabaseConfig::instance.database = settings.value("Database/database", "jupiter_db").toString();
    DatabaseConfig::instance.username = settings.value("Database/username", "root").toString();
    DatabaseConfig::instance.password = settings.value("Database/password", "").toString();
    DatabaseConfig::instance.port = settings.value("Database/port", 3306).toInt();

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
    mainInitialized = true;  // Ustaw flagę statyczną
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

bool DatabaseManager::registerUser(const QString& username, const QString& password)
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
            throw std::runtime_error("Username already exists: " + username.toStdString());
        }

        QString salt = generateSalt();
        QString hashedPassword = hashPassword(password + salt);

        QSqlQuery query(database);
        query.prepare(DatabaseQueries::Users::REGISTER);
        query.addBindValue(username);
        query.addBindValue(hashedPassword);
        query.addBindValue(salt);

        if (!query.exec()) {
            throw std::runtime_error("Failed to register user: " + query.lastError().text().toStdString());
        }

        if (!database.commit()) {
            throw std::runtime_error("Failed to commit registration");
        }

        qInfo() << "User registered successfully:" << username;
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

QVector<QPair<QString, QString>> DatabaseManager::getChatHistory(quint32 userId1, quint32 userId2, int limit)
{
    QVector<QPair<QString, QString>> messages;
    QString tableName = getChatTableName(userId1, userId2);

    if (!chatTableExists(tableName)) {
        return messages;
    }

    if (!database.transaction()) {
        qWarning() << "Failed to start transaction for chat history";
        return messages;
    }

    try {
        QSqlQuery query(database);
        query.prepare(DatabaseQueries::Messages::GET_CHAT_HISTORY.arg(tableName));
        query.addBindValue(limit);

        if (!query.exec()) {
            throw std::runtime_error("Failed to get chat history: " + query.lastError().text().toStdString());
        }

        while (query.next()) {
            QString sender = query.value(1).toString();
            QString message = query.value(2).toString();
            messages.append({sender, message});
        }

        if (!database.commit()) {
            throw std::runtime_error("Failed to commit chat history query");
        }

        return messages;
    }
    catch (const std::exception& e) {
        qWarning() << "Error getting chat history:" << e.what();
        database.rollback();
        return messages;
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
    if (username.length() < MIN_USERNAME_LENGTH || username.length() > MAX_USERNAME_LENGTH) {
        return false;
    }

    QRegularExpression rx("^[a-zA-Z0-9_-]+$");
    return rx.match(username).hasMatch();
}

bool DatabaseManager::validatePassword(const QString& password)
{
    return password.length() >= MIN_PASSWORD_LENGTH &&
           password.length() <= MAX_PASSWORD_LENGTH;
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
