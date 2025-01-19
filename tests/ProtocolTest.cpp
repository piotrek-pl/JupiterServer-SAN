/**
 * @file ProtocolTest.cpp
 * @brief Protocol test implementation
 * @author piotrek-pl
 * @date 2025-01-19 19:39:08
 */

#include "ProtocolTest.h"
#include "server/Protocol.h"
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

void ProtocolTest::initTestCase()
{
    qDebug() << "Starting Protocol tests";
}

void ProtocolTest::cleanupTestCase()
{
    qDebug() << "Protocol tests finished";
}

void ProtocolTest::testMessageCreation()
{
    auto loginMsg = Protocol::MessageStructure::createLoginRequest("testuser", "testpass");
    QCOMPARE(loginMsg["type"].toString(), Protocol::MessageType::LOGIN);
    QCOMPARE(loginMsg["username"].toString(), "testuser");
    QCOMPARE(loginMsg["password"].toString(), "testpass");
    QVERIFY(loginMsg.contains("protocol_version"));
}

void ProtocolTest::testLoginMessage()
{
    auto msg = Protocol::MessageStructure::createLoginRequest("user", "pass");
    QCOMPARE(msg["type"].toString(), Protocol::MessageType::LOGIN);
    QCOMPARE(msg["protocol_version"].toInt(), Protocol::PROTOCOL_VERSION);
}

void ProtocolTest::testChatMessage()
{
    auto msg = Protocol::MessageStructure::createMessage(1, "Hello");
    QCOMPARE(msg["type"].toString(), Protocol::MessageType::SEND_MESSAGE);
    QCOMPARE(msg["receiver_id"].toInt(), 1);
    QCOMPARE(msg["content"].toString(), "Hello");
    QVERIFY(msg.contains("timestamp"));
}

void ProtocolTest::testPingPong()
{
    auto ping = Protocol::MessageStructure::createPing();
    QCOMPARE(ping["type"].toString(), Protocol::MessageType::PING);
    QVERIFY(ping.contains("timestamp"));

    qint64 timestamp = ping["timestamp"].toInteger();
    auto pong = Protocol::MessageStructure::createPong(timestamp);
    QCOMPARE(pong["type"].toString(), Protocol::MessageType::PONG);
    QCOMPARE(pong["timestamp"].toInteger(), timestamp);
}

void ProtocolTest::testStatusUpdate()
{
    auto msg = Protocol::MessageStructure::createStatusUpdate("online");
    QCOMPARE(msg["type"].toString(), Protocol::MessageType::STATUS_UPDATE);
    QCOMPARE(msg["status"].toString(), "online");
    QVERIFY(msg.contains("timestamp"));
}

void ProtocolTest::testMessageAck()
{
    QString messageId = "test-id-123";
    auto msg = Protocol::MessageStructure::createMessageAck(messageId);
    QCOMPARE(msg["type"].toString(), Protocol::MessageType::MESSAGE_ACK);
    QCOMPARE(msg["message_id"].toString(), messageId);
    QVERIFY(msg.contains("timestamp"));
}

#include "moc_ProtocolTest.cpp"