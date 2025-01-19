#include "Message.h"
#include <QJsonDocument>
#include <QJsonObject>

Message::Message()
    : m_id(0)
    , m_senderId(0)
    , m_receiverId(0)
    , m_isDeleted(false)
{
}

Message::Message(quint64 id, quint32 senderId, quint32 receiverId, const QString& content)
    : m_id(id)
    , m_senderId(senderId)
    , m_receiverId(receiverId)
    , m_content(content)
    , m_sentAt(QDateTime::currentDateTime())
    , m_isDeleted(false)
{
}

QByteArray Message::toJson() const
{
    QJsonObject json;
    json["id"] = QString::number(m_id);
    json["sender_id"] = QString::number(m_senderId);
    json["receiver_id"] = QString::number(m_receiverId);
    json["content"] = m_content;
    json["sent_at"] = m_sentAt.toString(Qt::ISODate);

    if (m_readAt.isValid()) {
        json["read_at"] = m_readAt.toString(Qt::ISODate);
    }

    json["is_deleted"] = m_isDeleted;

    return QJsonDocument(json).toJson(QJsonDocument::Compact);
}

Message Message::fromJson(const QByteArray& json)
{
    QJsonDocument doc = QJsonDocument::fromJson(json);
    QJsonObject obj = doc.object();

    Message message;
    message.setId(obj["id"].toString().toULongLong());
    message.setSenderId(obj["sender_id"].toString().toUInt());
    message.setReceiverId(obj["receiver_id"].toString().toUInt());
    message.setContent(obj["content"].toString());
    message.setSentAt(QDateTime::fromString(obj["sent_at"].toString(), Qt::ISODate));

    if (obj.contains("read_at")) {
        message.setReadAt(QDateTime::fromString(obj["read_at"].toString(), Qt::ISODate));
    }

    if (obj["is_deleted"].toBool()) {
        message.markAsDeleted();
    }

    return message;
}
