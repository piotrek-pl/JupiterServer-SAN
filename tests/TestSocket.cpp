#include "TestSocket.h"
#include <QHostAddress>
#include <QDebug>
#include <QCoreApplication>
#include <QDateTime>
#include <QThread>

TestSocket::TestSocket(QObject *parent)
    : QTcpSocket(parent)
{
    qDebug() << QString("[%1] TestSocket created")
    .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz"));

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
    qDebug() << QString("[%1] TestSocket destroyed")
    .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz"));
    close();
}

qint64 TestSocket::bytesAvailable() const
{
    qint64 bytes = QTcpSocket::bytesAvailable() + buffer.size();
    qDebug() << QString("[%1] TestSocket::bytesAvailable returning: %2")
                    .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz"))
                    .arg(bytes);
    return bytes;
}

void TestSocket::simulateReceive(const QByteArray& data)
{
    qDebug() << QString("[%1] TestSocket::simulateReceive - Simulating receive of data: %2")
    .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz"))
        .arg(QString::fromUtf8(data));

    buffer = data;
    qDebug() << QString("[%1] TestSocket::simulateReceive - Buffer size after setting: %2")
                    .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz"))
                    .arg(buffer.size());

    // Emituj sygnał readyRead()
    emit readyRead();

    // Daj czas na przetworzenie
    for(int i = 0; i < 20; i++) {
        QCoreApplication::processEvents();
        QThread::msleep(1);
    }
}

qint64 TestSocket::readData(char *data, qint64 maxSize)
{
    qint64 size = qMin(maxSize, static_cast<qint64>(buffer.size()));
    if (size > 0) {
        memcpy(data, buffer.constData(), size);
        buffer.remove(0, size);
        qDebug() << QString("[%1] TestSocket::readData - Read %2 bytes, remaining buffer: %3")
                        .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz"))
                        .arg(size)
                        .arg(buffer.size());
        return size;
    }
    qDebug() << QString("[%1] TestSocket::readData - No data to read")
                    .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz"));
    return 0;
}

qint64 TestSocket::writeData(const char *data, qint64 maxSize)
{
    QByteArray written(data, maxSize);
    lastWrittenData = written;
    writtenData.append(written);

    qDebug() << QString("[%1] TestSocket::writeData - Written %2 bytes: %3")
                    .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz"))
                    .arg(maxSize)
                    .arg(QString::fromUtf8(written));

    emit bytesWritten(maxSize);
    return maxSize;
}

bool TestSocket::waitForResponse(int timeout)
{
    qDebug() << QString("[%1] TestSocket::waitForResponse - Starting wait with timeout: %2ms")
    .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz"))
        .arg(timeout);

    responseTimer.start();
    int lastReportTime = 0;

    while (responseTimer.elapsed() < timeout && lastWrittenData.isEmpty()) {
        QCoreApplication::processEvents();
        QThread::msleep(1);

        int currentTime = responseTimer.elapsed();
        if (currentTime - lastReportTime >= 100) {
            qDebug() << QString("[%1] TestSocket::waitForResponse - Still waiting... Elapsed: %2ms")
            .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz"))
                .arg(currentTime);
            lastReportTime = currentTime;
        }
    }

    bool hasResponse = !lastWrittenData.isEmpty();
    qDebug() << QString("[%1] TestSocket::waitForResponse - Finished. Success: %2 Elapsed: %3ms Response: %4")
                    .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz"))
                    .arg(hasResponse)
                    .arg(responseTimer.elapsed())
                    .arg(QString::fromUtf8(lastWrittenData));

    return hasResponse;
}
