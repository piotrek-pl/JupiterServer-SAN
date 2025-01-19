#include "User.h"

User::User()
    : m_id(0)
    , m_status(Status::Offline)
{
}

User::User(quint32 id, const QString& username)
    : m_id(id)
    , m_username(username)
    , m_status(Status::Offline)
{
}

QString User::statusToString(Status status)
{
    switch (status) {
    case Status::Online:
        return "online";
    case Status::Offline:
        return "offline";
    case Status::Away:
        return "away";
    case Status::Busy:
        return "busy";
    default:
        return "offline";
    }
}

User::Status User::stringToStatus(const QString& status)
{
    if (status == "online") return Status::Online;
    if (status == "away") return Status::Away;
    if (status == "busy") return Status::Busy;
    return Status::Offline;
}
