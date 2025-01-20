#include "DatabaseManager.h"
#include "DatabaseQueries.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSqlError>
#include <QDebug>
#include <QSettings>
#include <QFile>
#include <QDir>

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

    if (initialized) {
        qDebug() << "Baza danych jest już zainicjalizowana";
        return true;
    }

    if (!QFile::exists(configFilePath)) {
        qCritical() << "Plik konfiguracyjny bazy danych nie został znaleziony:" << configFilePath;
        return false;
    }

    QSettings settings(configFilePath, QSettings::IniFormat);
    settings.beginGroup("Database");

    QString hostname = settings.value("hostname", "localhost").toString();
    QString databaseName = settings.value("database", "jupiter_db").toString();
    QString username = settings.value("username", "root").toString();
    QString password = settings.value("password", "").toString();
    int port = settings.value("port", 3306).toInt();

    settings.endGroup();

    if (hostname.isEmpty() || databaseName.isEmpty() || username.isEmpty()) {
        qCritical() << "Brak wymaganych parametrów konfiguracji bazy danych";
        return false;
    }

    database = QSqlDatabase::addDatabase("QMYSQL");
    database.setHostName(hostname);
    database.setDatabaseName(databaseName);
    database.setUserName(username);
    database.setPassword(password);
    database.setPort(port);

    if (!database.open()) {
        qCritical() << "Nie udało się otworzyć bazy danych:" << database.lastError().text();
        return false;
    }

    if (!createTablesIfNotExist()) {
        qCritical() << "Nie udało się utworzyć tabel bazy danych";
        database.close();
        return false;
    }

    initialized = true;
    qInfo() << "Baza danych została pomyślnie zainicjalizowana";
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

        if (!query.exec(DatabaseQueries::Create::MESSAGES_TABLE)) {
            throw std::runtime_error("Failed to create messages table: " + query.lastError().text().toStdString());
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
    if (!database.transaction()) {
        qWarning() << "Failed to start transaction for user authentication";
        return false;
    }

    try {
        QSqlQuery query(database);
        query.prepare(DatabaseQueries::Users::AUTHENTICATE);
        query.addBindValue(username);

        if (!query.exec() || !query.next()) {
            throw std::runtime_error("Authentication failed for user: " + username.toStdString());
        }

        userId = query.value(0).toUInt();
        QString storedHash = query.value(1).toString();
        QString salt = query.value(2).toString();

        if (!verifyPassword(password + salt, storedHash)) {
            throw std::runtime_error("Invalid password for user: " + username.toStdString());
        }

        if (!updateUserStatus(userId, "online")) {
            throw std::runtime_error("Failed to update user status");
        }

        if (!database.commit()) {
            throw std::runtime_error("Failed to commit authentication");
        }

        qInfo() << "User authenticated successfully:" << username;
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

        if (!query.exec() || !query.next()) {
            qWarning() << "Failed to get user status:" << query.lastError().text();
            return false;
        }

        status = query.value(0).toString();
        return true;
    }
    catch (const std::exception& e) {
        qWarning() << "Error getting user status:" << e.what();
        return false;
    }
}

bool DatabaseManager::updateUserStatus(quint32 userId, const QString& status)
{
    if (!database.transaction()) {
        qWarning() << "Failed to start transaction for status update";
        return false;
    }

    try {
        QSqlQuery query(database);
        query.prepare(DatabaseQueries::Users::UPDATE_STATUS);
        query.addBindValue(status);
        query.addBindValue(userId);

        if (!query.exec()) {
            throw std::runtime_error("Failed to update user status: " + query.lastError().text().toStdString());
        }

        if (!database.commit()) {
            throw std::runtime_error("Failed to commit status update");
        }

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
    if (!database.transaction()) {
        qWarning() << "Failed to start transaction for storing message";
        return false;
    }

    try {
        if (!userExists(senderId) || !userExists(receiverId)) {
            throw std::runtime_error("Invalid sender or receiver ID");
        }

        QSqlQuery query(database);
        query.prepare(DatabaseQueries::Messages::STORE);
        query.addBindValue(senderId);
        query.addBindValue(receiverId);
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

    if (!database.transaction()) {
        qWarning() << "Failed to start transaction for getting friends list";
        return friendsList;
    }

    try {
        QString queryStr = DatabaseQueries::Friends::LIST.arg(userId);
        QSqlQuery query(database);

        if (!query.exec(queryStr)) {
            throw std::runtime_error("Failed to get friends list: " + query.lastError().text().toStdString());
        }

        while (query.next()) {
            quint32 friendId = query.value(0).toUInt();
            QString username = query.value(1).toString();
            friendsList.append({friendId, username});
        }

        if (!database.commit()) {
            throw std::runtime_error("Failed to commit friends list query");
        }

        return friendsList;
    }
    catch (const std::exception& e) {
        qWarning() << "Error getting friends list:" << e.what();
        database.rollback();
        return friendsList;
    }
}

QVector<QPair<QString, QString>> DatabaseManager::getChatHistory(quint32 userId, quint32 friendId, int limit)
{
    QVector<QPair<QString, QString>> messages;

    if (!database.transaction()) {
        qWarning() << "Failed to start transaction for chat history";
        return messages;
    }

    try {
        QSqlQuery query(database);
        query.prepare(DatabaseQueries::Messages::GET_HISTORY);
        query.addBindValue(userId);
        query.addBindValue(friendId);
        query.addBindValue(friendId);
        query.addBindValue(userId);
        query.addBindValue(limit);

        if (!query.exec()) {
            throw std::runtime_error("Failed to get chat history: " + query.lastError().text().toStdString());
        }

        while (query.next()) {
            QString sender = query.value(0).toString();
            QString message = query.value(1).toString();
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

bool DatabaseManager::verifyPassword(const QString& password, const QString& hash)
{
    return hashPassword(password) == hash;
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
