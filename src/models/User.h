#ifndef USER_H
#define USER_H

#include <QString>
#include <QDateTime>

class User
{
public:
    enum class Status {
        Offline,
        Online,
        Away,
        Busy
    };

    User();
    User(quint32 id, const QString& username);

    quint32 getId() const { return m_id; }
    QString getUsername() const { return m_username; }
    QString getEmail() const { return m_email; }
    Status getStatus() const { return m_status; }
    QDateTime getLastLogin() const { return m_lastLogin; }
    QDateTime getCreatedAt() const { return m_createdAt; }
    QString getNickname() const { return m_nickname; }

    void setId(quint32 id) { m_id = id; }
    void setUsername(const QString& username) { m_username = username; }
    void setEmail(const QString& email) { m_email = email; }
    void setStatus(Status status) { m_status = status; }
    void setLastLogin(const QDateTime& lastLogin) { m_lastLogin = lastLogin; }
    void setCreatedAt(const QDateTime& createdAt) { m_createdAt = createdAt; }
    void setNickname(const QString& nickname) { m_nickname = nickname; }

    static QString statusToString(Status status);
    static Status stringToStatus(const QString& status);

private:
    quint32 m_id;
    QString m_username;
    QString m_email;
    Status m_status;
    QDateTime m_lastLogin;
    QDateTime m_createdAt;
    QString m_nickname;
};

#endif // USER_H
