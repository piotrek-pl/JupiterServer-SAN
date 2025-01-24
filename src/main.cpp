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

        // Usuwanie wszystkich dynamicznych tabel chatów
        query.exec("SELECT TABLE_NAME FROM information_schema.tables "
                   "WHERE table_schema = DATABASE() "
                   "AND table_name LIKE 'chat_%_%'");
        while (query.next()) {
            QString tableName = query.value(0).toString();
            QSqlQuery dropQuery(database);
            if (!dropQuery.exec("DROP TABLE IF EXISTS " + tableName)) {
                qDebug() << "Drop chat table error:" << dropQuery.lastError().text();
            }
        }

        // Usuwanie tabel zaproszeń
        query.exec("SELECT TABLE_NAME FROM information_schema.tables "
                   "WHERE table_schema = DATABASE() "
                   "AND (table_name LIKE 'user_%_sent_invitations' "
                   "OR table_name LIKE 'user_%_received_invitations')");
        while (query.next()) {
            QString tableName = query.value(0).toString();
            QSqlQuery dropQuery(database);
            if (!dropQuery.exec("DROP TABLE IF EXISTS " + tableName)) {
                qDebug() << "Drop invitations table error:" << dropQuery.lastError().text();
            }
        }

        // Usuwanie pozostałych tabel
        if (!query.exec("DROP TABLE IF EXISTS user_1_friends")) {
            qDebug() << "Drop friends table error:" << query.lastError().text();
        }
        if (!query.exec("DROP TABLE IF EXISTS user_2_friends")) {
            qDebug() << "Drop friends table error:" << query.lastError().text();
        }
        if (!query.exec("DROP TABLE IF EXISTS user_3_friends")) {
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

        // Dodawanie użytkowników testowych
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

        // Dodaj test4
        query.prepare("INSERT INTO users (username, password, salt, status) VALUES (?, ?, ?, ?)");
        query.bindValue(0, "test4");
        query.bindValue(1, hashPassword("test4", salt));
        query.bindValue(2, salt);
        query.bindValue(3, "offline");

        if (!query.exec()) {
            throw std::runtime_error("Failed to add test4: " + query.lastError().text().toStdString());
        }

        // Dodaj test5
        query.prepare("INSERT INTO users (username, password, salt, status) VALUES (?, ?, ?, ?)");
        query.bindValue(0, "test5");
        query.bindValue(1, hashPassword("test5", salt));
        query.bindValue(2, salt);
        query.bindValue(3, "offline");

        if (!query.exec()) {
            throw std::runtime_error("Failed to add test5: " + query.lastError().text().toStdString());
        }

        // Dodaj test6
        query.prepare("INSERT INTO users (username, password, salt, status) VALUES (?, ?, ?, ?)");
        query.bindValue(0, "test6");
        query.bindValue(1, hashPassword("test6", salt));
        query.bindValue(2, salt);
        query.bindValue(3, "offline");

        if (!query.exec()) {
            throw std::runtime_error("Failed to add test6: " + query.lastError().text().toStdString());
        }

        // Tworzenie tabel zaproszeń dla wszystkich użytkowników
        qDebug() << "Creating invitation tables...";
        for(int i = 1; i <= 6; i++) {
            QString sentTableQuery = QString(
                                         "CREATE TABLE IF NOT EXISTS user_%1_sent_invitations ("
                                         "request_id INT AUTO_INCREMENT PRIMARY KEY, "
                                         "to_user_id INT NOT NULL, "
                                         "to_username VARCHAR(32) NOT NULL, "
                                         "status ENUM('pending', 'accepted', 'rejected', 'cancelled') DEFAULT 'pending', "
                                         "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, "
                                         "updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, "
                                         "FOREIGN KEY (to_user_id) REFERENCES users(id)"
                                         ") ENGINE=InnoDB;").arg(i);

            QString receivedTableQuery = QString(
                                             "CREATE TABLE IF NOT EXISTS user_%1_received_invitations ("
                                             "request_id INT AUTO_INCREMENT PRIMARY KEY, "
                                             "from_user_id INT NOT NULL, "
                                             "from_username VARCHAR(32) NOT NULL, "
                                             "status ENUM('pending', 'accepted', 'rejected', 'cancelled') DEFAULT 'pending', "
                                             "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, "
                                             "updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, "
                                             "FOREIGN KEY (from_user_id) REFERENCES users(id)"
                                             ") ENGINE=InnoDB;").arg(i);

            if (!query.exec(sentTableQuery)) {
                throw std::runtime_error("Failed to create sent invitations table for user " +
                                         QString::number(i).toStdString());
            }
            qDebug() << "Created new invitations table:" << QString("user_%1_sent_invitations").arg(i);

            if (!query.exec(receivedTableQuery)) {
                throw std::runtime_error("Failed to create received invitations table for user " +
                                         QString::number(i).toStdString());
            }
            qDebug() << "Created new invitations table:" << QString("user_%1_received_invitations").arg(i);
        }

        // Dodawanie relacji znajomych dla wszystkich użytkowników testowych
        qDebug() << "Adding friends relationships...";

        // Znajomi dla test1 (ID: 1)
        if (!db->addFriend(1, 2) || !db->addFriend(1, 3)) {
            throw std::runtime_error("Failed to add friends for test1");
        }

        // Znajomi dla test2 (ID: 2)
        if (!db->addFriend(2, 1) || !db->addFriend(2, 3)) {
            throw std::runtime_error("Failed to add friends for test2");
        }

        // Znajomi dla test3 (ID: 3)
        if (!db->addFriend(3, 1) || !db->addFriend(3, 2)) {
            throw std::runtime_error("Failed to add friends for test3");
        }

        // Dodawanie testowych wiadomości - 50 wiadomości między każdą parą użytkowników
        qDebug() << "Adding test messages...";

        QDateTime baseTime = QDateTime::currentDateTime().addDays(-30); // Zaczynamy od 30 dni temu

        // Wiadomości od test1 do test2 i test3
        for (int i = 0; i < 50; ++i) {
            QString messageToTest2 = QString("Wiadomość nr %1 od test1 do test2").arg(i + 1);
            QString messageToTest3 = QString("Wiadomość nr %1 od test1 do test3").arg(i + 1);

            if (!db->storeMessage(1, 2, messageToTest2)) {
                throw std::runtime_error("Failed to add message from test1 to test2");
            }
            if (!db->storeMessage(1, 3, messageToTest3)) {
                throw std::runtime_error("Failed to add message from test1 to test3");
            }
        }

        // Wiadomości od test2 do test1 i test3
        for (int i = 0; i < 50; ++i) {
            QString messageToTest1 = QString("Wiadomość nr %1 od test2 do test1").arg(i + 1);
            QString messageToTest3 = QString("Wiadomość nr %1 od test2 do test3").arg(i + 1);

            if (!db->storeMessage(2, 1, messageToTest1)) {
                throw std::runtime_error("Failed to add message from test2 to test1");
            }
            if (!db->storeMessage(2, 3, messageToTest3)) {
                throw std::runtime_error("Failed to add message from test2 to test3");
            }
        }

        // Wiadomości od test3 do test1 i test2
        for (int i = 0; i < 50; ++i) {
            QString messageToTest1 = QString("Wiadomość nr %1 od test3 do test1").arg(i + 1);
            QString messageToTest2 = QString("Wiadomość nr %1 od test3 do test2").arg(i + 1);

            if (!db->storeMessage(3, 1, messageToTest1)) {
                throw std::runtime_error("Failed to add message from test3 to test1");
            }
            if (!db->storeMessage(3, 2, messageToTest2)) {
                throw std::runtime_error("Failed to add message from test3 to test2");
            }
        }

        if (!database.commit()) {
            throw std::runtime_error("Failed to commit transaction");
        }

        qDebug() << "Test data filled successfully";
        return;
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
