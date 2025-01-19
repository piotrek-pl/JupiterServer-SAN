/**
 * @file TestSocket.cpp
 * @brief Mock socket implementation for testing
 * @author piotrek-pl
 * @date 2025-01-19 21:16:31
 */

#include "TestSocket.h"
#include <QHostAddress>
#include <QDebug>
#include <QCoreApplication>

TestSocket::TestSocket(QObject *parent)
    : QTcpSocket(parent)
{
    // Symulujemy połączony socket
    setSocketState(QAbstractSocket::ConnectedState);
    setLocalAddress(QHostAddress::LocalHost);
    setLocalPort(12345);
    setPeerAddress(QHostAddress::LocalHost);
    setPeerPort(54321);
    setOpenMode(QIODevice::ReadWrite);
}

TestSocket::~TestSocket()
{
    close();
}

void TestSocket::simulateReceive(const QByteArray& data)
{
    buffer = data;
    emit readyRead();

    // Dodaj opóźnienie, aby dać czas na przetworzenie
    QCoreApplication::processEvents();
}

qint64 TestSocket::writeData(const char *data, qint64 maxSize)
{
    lastWrittenData = QByteArray(data, maxSize);
    emit bytesWritten(maxSize);
    return maxSize;
}

qint64 TestSocket::readData(char *data, qint64 maxSize)
{
    qint64 size = qMin(maxSize, static_cast<qint64>(buffer.size()));
    if (size > 0) {
        memcpy(data, buffer.constData(), size);
        buffer.remove(0, size);
        return size;
    }
    return 0;
}
