#ifndef TESTSOCKET_H
#define TESTSOCKET_H

#include <QTcpSocket>
#include <QByteArray>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>

class TestSocket : public QTcpSocket
{
    Q_OBJECT
public:
    explicit TestSocket(QObject *parent = nullptr);
    ~TestSocket() override;

    void simulateReceive(const QByteArray& data);
    bool waitForResponse(int timeout = 1000);
    QList<QByteArray> writtenData; // Lista wszystkich napisanych danych
    QByteArray lastWrittenData;

    void clearResponses() {
        writtenData.clear();
        lastWrittenData.clear();
    }

    QByteArray getResponse(const QString& type) {
        for (const QByteArray& response : writtenData) {
            QJsonDocument doc = QJsonDocument::fromJson(response);
            if (doc.isObject() && doc.object()["type"].toString() == type) {
                return response;
            }
        }
        return QByteArray();
    }

protected:
    qint64 writeData(const char *data, qint64 maxSize) override;
    qint64 readData(char *data, qint64 maxSize) override;
    qint64 bytesAvailable() const override; // Dodajemy tę metodę

private:
    QByteArray buffer;
    QElapsedTimer responseTimer;
};

#endif // TESTSOCKET_H
