/**
 * @file ClientSessionTest.h
 * @brief ClientSession test class definition
 * @author piotrek-pl
 * @date 2025-01-19 19:43:07
 */

#ifndef CLIENTSESSIONTEST_H
#define CLIENTSESSIONTEST_H

#include <QObject>
#include <QtTest>
#include "server/ClientSession.h"
#include "database/DatabaseManager.h"
#include "TestSocket.h"

class ClientSessionTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void testConnectionInitialization();
    void testAuthentication();
    void testMessageHandling();
    void testPingPongMechanism();
    void testStatusUpdate();
    void testMessageAcknowledgement();

private:
    TestSocket* socket;
    DatabaseManager* dbManager;
    ClientSession* session;

    // Helper methods
    QJsonObject createLoginMessage(const QString& username, const QString& password);
    QJsonObject createChatMessage(int receiverId, const QString& content);
    void verifyResponse(const QByteArray& response, const QString& expectedType);
};

#endif // CLIENTSESSIONTEST_H
