#ifndef MESSAGE_H
#define MESSAGE_H

#include <QString>
#include <QDateTime>

class Message
{
public:
    Message();
    Message(quint64 id, quint32 senderId, quint32 receiverId, const QString& content);

    quint64 getId() const { return m_id; }
    quint32 getSenderId() const { return m_senderId; }
    quint32 getReceiverId() const { return m_receiverId; }
    QString getContent() const { return m_content; }
    QDateTime getSentAt() const { return m_sentAt; }
    QDateTime getReadAt() const { return m_readAt; }
    bool isRead() const { return m_readAt.isValid(); }
    bool isDeleted() const { return m_isDeleted; }

    void setId(quint64 id) { m_id = id; }
    void setSenderId(quint32 senderId) { m_senderId = senderId; }
    void setReceiverId(quint32 receiverId) { m_receiverId = receiverId; }
    void setContent(const QString& content) { m_content = content; }
    void setSentAt(const QDateTime& sentAt) { m_sentAt = sentAt; }
    void setReadAt(const QDateTime& readAt) { m_readAt = readAt; }
    void markAsRead() { m_readAt = QDateTime::currentDateTime(); }
    void markAsDeleted() { m_isDeleted = true; }

    // Metody do serializacji/deserializacji
    QByteArray toJson() const;
    static Message fromJson(const QByteArray& json);

private:
    quint64 m_id;
    quint32 m_senderId;
    quint32 m_receiverId;
    QString m_content;
    QDateTime m_sentAt;
    QDateTime m_readAt;
    bool m_isDeleted;
};

#endif // MESSAGE_H
