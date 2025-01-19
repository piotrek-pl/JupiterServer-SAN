#include <QCoreApplication>
#include <QDebug>
#include "server/Server.h"
#include "database/DatabaseManager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QCryptographicHash>

QString hashPassword(const QString& password, const QString& salt) {
    QString combinedPassword = password + salt;
    QByteArray hash = QCryptographicHash::hash(
        combinedPassword.toUtf8(),
        QCryptographicHash::Sha256
        );
    return QString(hash.toHex());
}

void fillTestData(DatabaseManager* db) {
    if (!db->isInitialized()) {
        qWarning() << "Database not initialized!";
        return;
    }

    QSqlDatabase database = db->getDatabase();
    if (!database.transaction()) {
        qWarning() << "Failed to start transaction for test data";
        return;
    }

    try {
        QSqlQuery query(database);

        // Czyszczenie starych danych w bezpiecznej kolejności
        qDebug() << "Cleaning old data...";
        query.exec("SET FOREIGN_KEY_CHECKS = 0");

        if (!query.exec("DELETE FROM messages_history")) {
            qDebug() << "Clean messages error:" << query.lastError().text();
        }
        if (!query.exec("DROP TABLE IF EXISTS user_1_friends")) {
            qDebug() << "Drop friends table error:" << query.lastError().text();
        }
        if (!query.exec("DELETE FROM user_sessions")) {
            qDebug() << "Clean sessions error:" << query.lastError().text();
        }
        if (!query.exec("DELETE FROM users")) {
            qDebug() << "Clean users error:" << query.lastError().text();
        }

        query.exec("SET FOREIGN_KEY_CHECKS = 1");

        // Resetowanie auto_increment
        qDebug() << "Resetting auto_increment...";
        if (!query.exec("ALTER TABLE users AUTO_INCREMENT = 1")) {
            qDebug() << "Reset auto_increment error:" << query.lastError().text();
        }

        // Dodawanie użytkowników
        qDebug() << "Adding test users...";
        const QString salt = "testSalt123";

        // Dodaj test1
        query.prepare("INSERT INTO users (username, password, salt, status) VALUES (?, ?, ?, ?)");
        query.bindValue(0, "test1");
        query.bindValue(1, hashPassword("test1", salt));
        query.bindValue(2, salt);
        query.bindValue(3, "online");

        if (!query.exec()) {
            throw std::runtime_error("Failed to add test1: " + query.lastError().text().toStdString());
        }

        // Dodaj test2
        query.prepare("INSERT INTO users (username, password, salt, status) VALUES (?, ?, ?, ?)");
        query.bindValue(0, "test2");
        query.bindValue(1, hashPassword("test2", salt));
        query.bindValue(2, salt);
        query.bindValue(3, "online");

        if (!query.exec()) {
            throw std::runtime_error("Failed to add test2: " + query.lastError().text().toStdString());
        }

        // Dodaj test3
        query.prepare("INSERT INTO users (username, password, salt, status) VALUES (?, ?, ?, ?)");
        query.bindValue(0, "test3");
        query.bindValue(1, hashPassword("test3", salt));
        query.bindValue(2, salt);
        query.bindValue(3, "offline");

        if (!query.exec()) {
            throw std::runtime_error("Failed to add test3: " + query.lastError().text().toStdString());
        }

        // Tworzenie i wypełnianie tabeli znajomych
        qDebug() << "Creating friends table...";
        QString createFriendsTable =
            "CREATE TABLE IF NOT EXISTS user_1_friends ("
            "friend_id INT NOT NULL,"
            "FOREIGN KEY (friend_id) REFERENCES users(id) ON DELETE CASCADE"
            ") ENGINE=InnoDB;";

        if (!query.exec(createFriendsTable)) {
            throw std::runtime_error("Failed to create friends table: " + query.lastError().text().toStdString());
        }

        // Dodawanie znajomych
        qDebug() << "Adding friends...";
        if (!query.exec("INSERT INTO user_1_friends (friend_id) VALUES (2)")) {
            throw std::runtime_error("Failed to add friend 2: " + query.lastError().text().toStdString());
        }

        if (!query.exec("INSERT INTO user_1_friends (friend_id) VALUES (3)")) {
            throw std::runtime_error("Failed to add friend 3: " + query.lastError().text().toStdString());
        }

        // Dodawanie wiadomości
        qDebug() << "Adding test messages...";
        query.prepare("INSERT INTO messages_history (sender_id, receiver_id, message) VALUES (?, ?, ?)");

        // Wiadomość 1
        query.bindValue(0, 1);
        query.bindValue(1, 2);
        query.bindValue(2, "Cześć, to wiadomość testowa 1");

        if (!query.exec()) {
            throw std::runtime_error("Failed to add message 1: " + query.lastError().text().toStdString());
        }

        // Wiadomość 2
        query.bindValue(0, 2);
        query.bindValue(1, 1);
        query.bindValue(2, "Hej, to odpowiedź testowa");

        if (!query.exec()) {
            throw std::runtime_error("Failed to add message 2: " + query.lastError().text().toStdString());
        }

        if (!database.commit()) {
            throw std::runtime_error("Failed to commit test data");
        }

        qInfo() << "Test data added successfully";
        qInfo() << "Created test users: test1, test2 (online) and test3 (offline)";
        qInfo() << "Added test messages between test1 and test2";
    }
    catch (const std::exception& e) {
        qWarning() << "Error filling test data:" << e.what();
        database.rollback();
    }
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QCoreApplication::setApplicationName("JupiterServer");
    QCoreApplication::setApplicationVersion("2.0");

    qInfo() << "Initializing JupiterServer v2.0...";

    DatabaseManager dbManager;
    if (!dbManager.init()) {
        qCritical() << "Failed to initialize database";
        return 1;
    }

    qInfo() << "Database initialized successfully";
    fillTestData(&dbManager);

    Server server;
    if (!server.start(1234)) {
        qCritical() << "Failed to start server";
        return 1;
    }

    qInfo() << "Server started successfully";
    qInfo() << "Listening on port 1234";
    qInfo() << "Test users available:";
    qInfo() << " - test1 (online)";
    qInfo() << " - test2 (online)";
    qInfo() << " - test3 (offline)";
    qInfo() << "Press Ctrl+C to quit";

    return app.exec();
}
