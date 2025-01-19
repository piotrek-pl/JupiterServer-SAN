/**
 * @file ClientSessionTest.cpp
 * @brief ClientSession test implementation
 * @author piotrek-pl
 * @date 2025-01-19 20:07:15
 */

#include "ClientSessionTest.h"
#include "server/Protocol.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QSettings>

void ClientSessionTest::initTestCase()
{
    // Utworzenie instancji QCoreApplication dla testów bazy danych
    if (!QCoreApplication::instance()) {
        static int argc = 1;
        static char* argv[] = { const_cast<char*>("test") };
        new QCoreApplication(argc, argv);
    }

    // Tworzenie testowego pliku konfiguracyjnego
    QString configDir = QDir::current().filePath("config");
    if (!QDir(configDir).exists()) {
        QDir().mkpath(configDir);
    }

    QString configPath = QDir::current().filePath("config/database.conf");
    QSettings settings(configPath, QSettings::IniFormat);
    settings.beginGroup("Database");
    settings.setValue("hostname", "localhost");
    settings.setValue("database", "jupiter_test");
    settings.setValue("username", "root");
    settings.setValue("password", "");
    settings.setValue("port", 3306);
    settings.endGroup();
    settings.sync();

    // Usunięcie istniejących połączeń
    const QString connectionName = QSqlDatabase::defaultConnection;
    if (QSqlDatabase::contains(connectionName)) {
        QSqlDatabase::removeDatabase(connectionName);
    }

    // Utworzenie instancji DatabaseManager
    dbManager = new DatabaseManager(this);

    // Inicjalizacja bazy danych
    if (!dbManager->init()) {
        qDebug() << "Failed to initialize DatabaseManager";
        QFAIL("DatabaseManager initialization failed");
    }

    // Czyszczenie bazy przed testami
    QSqlDatabase db = dbManager->getDatabase();
    QSqlQuery query(db);

    // Wyłączenie sprawdzania kluczy obcych
    query.exec("SET FOREIGN_KEY_CHECKS = 0");

    // Usuwanie istniejących tabel w odpowiedniej kolejności
    if (!query.exec("DROP TABLE IF EXISTS messages")) {
        qDebug() << "Failed to drop messages table:" << query.lastError().text();
    }
    if (!query.exec("DROP TABLE IF EXISTS user_sessions")) {
        qDebug() << "Failed to drop user_sessions table:" << query.lastError().text();
    }
    if (!query.exec("DROP TABLE IF EXISTS users")) {
        qDebug() << "Failed to drop users table:" << query.lastError().text();
    }

    // Włączenie z powrotem sprawdzania kluczy obcych
    query.exec("SET FOREIGN_KEY_CHECKS = 1");

    // Inicjalizacja tabel na nowo
    if (!dbManager->createTablesIfNotExist()) {
        qDebug() << "Failed to create tables";
        QFAIL("Failed to create database tables");
    }

    // Dodanie testowego użytkownika
    if (!dbManager->registerUser("testuser", "testpass")) {
        qDebug() << "Failed to register test user";
        QFAIL("Could not create test user");
    }

    qDebug() << "Test database initialized successfully";
}

void ClientSessionTest::cleanupTestCase()
{
    if (dbManager) {
        // Czyszczenie bazy
        QSqlQuery query(dbManager->getDatabase());

        // Wyłączenie sprawdzania kluczy obcych
        query.exec("SET FOREIGN_KEY_CHECKS = 0");

        // Usuwanie tabel w odpowiedniej kolejności
        query.exec("DROP TABLE IF EXISTS messages");
        query.exec("DROP TABLE IF EXISTS user_sessions");
        query.exec("DROP TABLE IF EXISTS users");

        // Włączenie sprawdzania kluczy obcych
        query.exec("SET FOREIGN_KEY_CHECKS = 1");

        delete dbManager;
        dbManager = nullptr;
    }

    // Usuń plik konfiguracyjny
    QFile::remove(QDir::current().filePath("config/database.conf"));
    QDir().rmdir(QDir::current().filePath("config"));

    QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
    qDebug() << "Test database cleaned up";
}

void ClientSessionTest::init()
{
    socket = new TestSocket(this);
    QVERIFY(socket != nullptr);

    // Upewnij się, że socket jest w odpowiednim stanie
    QVERIFY(socket->isValid());

    session = new ClientSession(socket, dbManager, this);
    qDebug() << "New client session created";

    // Najpierw zaloguj użytkownika
    QJsonObject loginMsg = Protocol::MessageStructure::createLoginRequest("testuser", "testpass");
    socket->simulateReceive(QJsonDocument(loginMsg).toJson());
}

void ClientSessionTest::cleanup()
{
    delete session;
    delete socket;
    qDebug() << "Test case cleaned up";
}

void ClientSessionTest::testConnectionInitialization()
{
    QVERIFY(socket != nullptr);
    QVERIFY(session != nullptr);
    QVERIFY(socket->isValid());
    qDebug() << "Connection initialization test passed";
}

void ClientSessionTest::testAuthentication()
{
    // Przygotowanie wiadomości logowania
    QJsonObject loginMsg = createLoginMessage("testuser", "testpass");
    QByteArray data = QJsonDocument(loginMsg).toJson();

    // Symulacja otrzymania danych
    socket->simulateReceive(data);

    // Weryfikacja odpowiedzi
    verifyResponse(socket->lastWrittenData, Protocol::MessageType::LOGIN_RESPONSE);

    // Sprawdzenie nieprawidłowego logowania
    loginMsg = createLoginMessage("wronguser", "wrongpass");
    data = QJsonDocument(loginMsg).toJson();
    socket->simulateReceive(data);
    verifyResponse(socket->lastWrittenData, Protocol::MessageType::ERROR);

    qDebug() << "Authentication test passed";
}

void ClientSessionTest::testMessageHandling()
{
    quint32 userId;
    // Najpierw zaloguj
    QVERIFY(dbManager->authenticateUser("testuser", "testpass", userId));

    QJsonObject loginMsg = createLoginMessage("testuser", "testpass");
    socket->simulateReceive(QJsonDocument(loginMsg).toJson());

    // Wyślij wiadomość
    QJsonObject chatMsg = createChatMessage(userId, "Test message");
    socket->simulateReceive(QJsonDocument(chatMsg).toJson());

    verifyResponse(socket->lastWrittenData, Protocol::MessageType::MESSAGE_ACK);
    qDebug() << "Message handling test passed";
}

// ClientSessionTest.cpp
void ClientSessionTest::testPingPongMechanism()
{
    QVERIFY(socket != nullptr);
    qDebug() << "Starting ping/pong test...";

    // Przygotuj wiadomość ping
    QJsonObject pingMsg = Protocol::MessageStructure::createPing();
    QJsonDocument doc(pingMsg);
    QByteArray message = doc.toJson();

    qDebug() << "Sending ping message:" << QString::fromUtf8(message);

    // Symuluj otrzymanie wiadomości ping
    socket->simulateReceive(message);

    // Sprawdź odpowiedź
    QByteArray response = socket->lastWrittenData;
    qDebug() << "Received response:" << QString::fromUtf8(response);

    QJsonParseError error;
    QJsonDocument responseDoc = QJsonDocument::fromJson(response, &error);
    QVERIFY2(error.error == QJsonParseError::NoError, "Failed to parse response JSON");

    QJsonObject responseObj = responseDoc.object();
    QVERIFY2(responseObj.contains("type"), "Response doesn't contain 'type' field");
    QCOMPARE(responseObj["type"].toString(), Protocol::MessageType::PONG);
    QVERIFY2(responseObj.contains("timestamp"), "Response doesn't contain 'timestamp' field");
    QCOMPARE(responseObj["timestamp"].toInteger(), pingMsg["timestamp"].toInteger());

    qDebug() << "Ping/Pong test completed successfully";
}

void ClientSessionTest::testStatusUpdate()
{
    quint32 userId;
    // Najpierw zaloguj
    QVERIFY(dbManager->authenticateUser("testuser", "testpass", userId));

    // Sprawdź czy status został zaktualizowany
    QString status;
    QVERIFY(dbManager->getUserStatus(userId, status));
    QCOMPARE(status, QString("online"));

    qDebug() << "Status update test passed";
}

void ClientSessionTest::testMessageAcknowledgement()
{
    quint32 userId;
    // Zaloguj użytkownika
    QVERIFY(dbManager->authenticateUser("testuser", "testpass", userId));

    QJsonObject loginMsg = createLoginMessage("testuser", "testpass");
    socket->simulateReceive(QJsonDocument(loginMsg).toJson());

    // Wyślij wiadomość
    QJsonObject chatMsg = createChatMessage(userId, "Test message");
    socket->simulateReceive(QJsonDocument(chatMsg).toJson());

    // Pobierz ID wiadomości z odpowiedzi
    QJsonDocument response = QJsonDocument::fromJson(socket->lastWrittenData);
    QString messageId = response.object()["message_id"].toString();

    // Wyślij potwierdzenie
    QJsonObject ackMsg = Protocol::MessageStructure::createMessageAck(messageId);
    socket->simulateReceive(QJsonDocument(ackMsg).toJson());

    qDebug() << "Message acknowledgement test passed";
}

// Helper methods
QJsonObject ClientSessionTest::createLoginMessage(const QString& username, const QString& password)
{
    return Protocol::MessageStructure::createLoginRequest(username, password);
}

QJsonObject ClientSessionTest::createChatMessage(int receiverId, const QString& content)
{
    return Protocol::MessageStructure::createMessage(receiverId, content);
}

void ClientSessionTest::verifyResponse(const QByteArray& response, const QString& expectedType)
{
    if (response.isEmpty()) {
        qWarning() << "Empty response received";
        QFAIL("Empty response received");
    }

    QJsonDocument doc = QJsonDocument::fromJson(response);
    if (doc.isNull()) {
        qWarning() << "Invalid JSON response:" << response;
        QFAIL("Invalid JSON response");
    }

    QVERIFY(doc.isObject());

    QJsonObject obj = doc.object();
    QVERIFY(obj.contains("type"));
    QCOMPARE(obj["type"].toString(), expectedType);
}

#include "moc_ClientSessionTest.cpp"
