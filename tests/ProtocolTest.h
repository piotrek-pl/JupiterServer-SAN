/**
 * @file ProtocolTest.h
 * @brief Protocol test class definition
 * @author piotrek-pl
 * @date 2025-01-19 17:01:01
 */

#ifndef PROTOCOLTEST_H
#define PROTOCOLTEST_H

#include <QObject>
#include <QtTest>

class ProtocolTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    
    void testMessageCreation();
    void testLoginMessage();
    void testChatMessage();
    void testPingPong();
    void testStatusUpdate();
    void testMessageAck();
};

#endif // PROTOCOLTEST_H