// TestSocket.h
/**
 * @file TestSocket.h
 * @brief Mock socket class for testing
 * @author piotrek-pl
 * @date 2025-01-19 21:03:50
 */

#ifndef TESTSOCKET_H
#define TESTSOCKET_H

#include <QTcpSocket>
#include <QByteArray>

class TestSocket : public QTcpSocket
{
    Q_OBJECT
public:
    explicit TestSocket(QObject *parent = nullptr);
    ~TestSocket() override;

    void simulateReceive(const QByteArray& data);
    QByteArray lastWrittenData;

protected:
    qint64 writeData(const char *data, qint64 maxSize) override;
    qint64 readData(char *data, qint64 maxSize) override;

private:
    QByteArray buffer;
};

#endif // TESTSOCKET_H
