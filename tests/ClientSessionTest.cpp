/**
 * @file ClientSessionTest.cpp
 * @brief ClientSession test implementation
 * @author piotrek-pl
 * @date 2025-01-19 20:07:15
 */

#include "ClientSessionTest.h"
#include "TestDatabaseQueries.h"
#include "network/Protocol.h"
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

    // Inicjalizacja bazy danych z konfiguracją testową
    dbManager = new DatabaseManager(TestDatabaseQueries::Config::CONFIG_FILE, this);

    if (!dbManager->init()) {
        qDebug() << "Failed to initialize DatabaseManager";
        QFAIL("DatabaseManager initialization failed");
    }

    // Czyszczenie bazy przed testami
    QSqlDatabase db = dbManager->getDatabase();
    QSqlQuery query(db);

    query.exec(TestDatabaseQueries::Setup::DISABLE_FOREIGN_KEYS);

    if (!query.exec(TestDatabaseQueries::Setup::DROP_MESSAGES_TABLE)) {
        qDebug() << "Failed to drop messages table:" << query.lastError().text();
    }
    if (!query.exec(TestDatabaseQueries::Setup::DROP_USER_SESSIONS_TABLE)) {
        qDebug() << "Failed to drop user_sessions table:" << query.lastError().text();
    }
    if (!query.exec(TestDatabaseQueries::Setup::DROP_USERS_TABLE)) {
        qDebug() << "Failed to drop users table:" << query.lastError().text();
    }

    query.exec(TestDatabaseQueries::Setup::ENABLE_FOREIGN_KEYS);

    // Używamy nowej metody do reinicjalizacji tabel
    if (!dbManager->reinitializeTables()) {
        qDebug() << "Failed to create tables";
        QFAIL("Failed to create database tables");
    }

    // Dodanie testowego użytkownika
    if (!dbManager->registerUser("testuser", "testpass", "testuser@test.com")) {
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

        query.exec(TestDatabaseQueries::Setup::DISABLE_FOREIGN_KEYS);

        query.exec(TestDatabaseQueries::Setup::DROP_MESSAGES_TABLE);
        query.exec(TestDatabaseQueries::Setup::DROP_USER_SESSIONS_TABLE);
        query.exec(TestDatabaseQueries::Setup::DROP_USERS_TABLE);

        query.exec(TestDatabaseQueries::Setup::ENABLE_FOREIGN_KEYS);

        delete dbManager;
        dbManager = nullptr;
    }

    QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
    qDebug() << "Test database cleaned up";
}

void ClientSessionTest::init()
{
    socket = new TestSocket(this);
    QVERIFY(socket != nullptr);

    socket->clearResponses(); // Wyczyść poprzednie odpowiedzi

    session = new ClientSession(socket, dbManager, this);
    qDebug() << "New client session created";
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
    qDebug() << "[TEST] Starting authentication test";

    // Przygotowanie wiadomości logowania
    QJsonObject loginMsg = createLoginMessage("testuser", "testpass");
    QByteArray data = QJsonDocument(loginMsg).toJson();

    // Wyczyść poprzednie dane
    socket->lastWrittenData.clear();

    qDebug() << "[TEST] Sending login message";

    // Symulacja otrzymania danych
    socket->simulateReceive(data);

    // Zwiększmy timeout do 5 sekund dla debugowania
    qDebug() << "[TEST] Waiting for login response";
    if (!socket->waitForResponse(5000)) {
        qDebug() << "[TEST] No response received within timeout";
        QFAIL("Timeout waiting for login response");
    }

    qDebug() << "[TEST] Received response, verifying";
    verifyResponse(socket->lastWrittenData, Protocol::MessageType::LOGIN_RESPONSE);

    // Test nieprawidłowego logowania
    socket->lastWrittenData.clear();
    loginMsg = createLoginMessage("wronguser", "wrongpass");
    data = QJsonDocument(loginMsg).toJson();

    qDebug() << "[TEST] Sending invalid login message";
    socket->simulateReceive(data);

    qDebug() << "[TEST] Waiting for error response";
    if (!socket->waitForResponse(5000)) {
        qDebug() << "[TEST] No error response received within timeout";
        QFAIL("Timeout waiting for error response");
    }

    qDebug() << "[TEST] Received error response, verifying";
    verifyResponse(socket->lastWrittenData, Protocol::MessageType::ERROR);

    qDebug() << "[TEST] Authentication test completed successfully";
}

void ClientSessionTest::testMessageHandling()
{
    quint32 userId;
    // Najpierw zaloguj
    QVERIFY(dbManager->authenticateUser("testuser", "testpass", userId));

    // Wyczyść poprzednie dane
    socket->lastWrittenData.clear();

    // Logowanie
    QJsonObject loginMsg = createLoginMessage("testuser", "testpass");
    socket->simulateReceive(QJsonDocument(loginMsg).toJson());
    QVERIFY2(socket->waitForResponse(), "Timeout waiting for login response");

    // Wyczyść odpowiedź logowania
    socket->lastWrittenData.clear();

    // Wyślij wiadomość
    QJsonObject chatMsg = createChatMessage(userId, "Test message");
    socket->simulateReceive(QJsonDocument(chatMsg).toJson());

    QVERIFY2(socket->waitForResponse(), "Timeout waiting for message response");
    verifyResponse(socket->lastWrittenData, Protocol::MessageType::MESSAGE_ACK);

    qDebug() << "Message handling test passed";
}

// ClientSessionTest.cpp
void ClientSessionTest::testPingPongMechanism()
{
    QVERIFY(socket != nullptr);
    qDebug() << "Starting ping/pong test...";

    // Wyczyść poprzednie dane
    socket->lastWrittenData.clear();

    // Przygotuj wiadomość ping
    QJsonObject pingMsg = Protocol::MessageStructure::createPing();
    QJsonDocument doc(pingMsg);
    QByteArray message = doc.toJson();

    qDebug() << "Sending ping message:" << QString::fromUtf8(message);

    // Symuluj otrzymanie wiadomości ping
    socket->simulateReceive(message);

    // Czekaj na odpowiedź
    QVERIFY2(socket->waitForResponse(), "Timeout waiting for ping response");

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
    qDebug() << QString("[%1] Starting status update test for user %2")
                    .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"))
                    .arg("piotrek-pl");

    // Najpierw zaloguj
    QVERIFY(dbManager->authenticateUser("testuser", "testpass", userId));

    // Sprawdź czy status został zaktualizowany
    QString status;
    QVERIFY(dbManager->getUserStatus(userId, status));
    QCOMPARE(status, QString("online"));

    qDebug() << QString("[%1] Status update test passed for user %2")
                    .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"))
                    .arg("piotrek-pl");
}

void ClientSessionTest::testMessageAcknowledgement()
{
    quint32 userId;
    qDebug() << QString("[%1] Starting message acknowledgement test for user %2")
                    .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"))
                    .arg("piotrek-pl");

    // Zaloguj użytkownika
    QVERIFY(dbManager->authenticateUser("testuser", "testpass", userId));

    socket->lastWrittenData.clear();

    QJsonObject loginMsg = createLoginMessage("testuser", "testpass");
    socket->simulateReceive(QJsonDocument(loginMsg).toJson());
    QVERIFY2(socket->waitForResponse(), "Timeout waiting for login response");

    socket->lastWrittenData.clear();

    // Wyślij wiadomość
    QJsonObject chatMsg = createChatMessage(userId, "Test message");
    socket->simulateReceive(QJsonDocument(chatMsg).toJson());
    QVERIFY2(socket->waitForResponse(), "Timeout waiting for chat message response");

    // Pobierz ID wiadomości z odpowiedzi
    QJsonDocument response = QJsonDocument::fromJson(socket->lastWrittenData);
    QString messageId = response.object()["message_id"].toString();

    socket->lastWrittenData.clear();

    // Wyślij potwierdzenie
    QJsonObject ackMsg = Protocol::MessageStructure::createMessageAck(messageId);
    socket->simulateReceive(QJsonDocument(ackMsg).toJson());
    QVERIFY2(socket->waitForResponse(), "Timeout waiting for ack response");

    qDebug() << QString("[%1] Message acknowledgement test passed for user %2")
                    .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"))
                    .arg("piotrek-pl");
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
    QByteArray targetResponse = static_cast<TestSocket*>(socket)->getResponse(expectedType);

    if (targetResponse.isEmpty()) {
        QString errorMsg = QString("[%1] No response of type '%2' received for user %3")
        .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"))
            .arg(expectedType)
            .arg("piotrek-pl");
        qWarning() << errorMsg;
        qWarning() << "Available responses:";
        for (const QByteArray& resp : static_cast<TestSocket*>(socket)->writtenData) {
            qWarning() << QString::fromUtf8(resp);
        }
        QFAIL(qPrintable(errorMsg));
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(targetResponse, &parseError);
    if (doc.isNull()) {
        QString errorMsg = QString("[%1] Invalid JSON response for user %2: %3\nResponse: %4")
        .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"))
            .arg("piotrek-pl")
            .arg(parseError.errorString())
            .arg(QString::fromUtf8(targetResponse));
        qWarning() << errorMsg;
        QFAIL(qPrintable(errorMsg));
    }

    QJsonObject obj = doc.object();
    QString actualType = obj["type"].toString();
    if (actualType != expectedType) {
        QString errorMsg = QString("[%1] Response type mismatch for user %2\nExpected: %3\nActual: %4\nFull response: %5")
        .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"))
            .arg("piotrek-pl")
            .arg(expectedType)
            .arg(actualType)
            .arg(QString::fromUtf8(targetResponse));
        qWarning() << errorMsg;
        QFAIL(qPrintable(errorMsg));
    }

    qDebug() << QString("[%1] Verified response for user %2: %3")
                    .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"))
                    .arg("piotrek-pl")
                    .arg(QString::fromUtf8(targetResponse));
}

#include "moc_ClientSessionTest.cpp"
