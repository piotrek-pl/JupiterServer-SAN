// ActiveSessions.h
#ifndef ACTIVESESSIONS_H
#define ACTIVESESSIONS_H

#include <QMap>
#include "ClientSession.h"

class ActiveSessions {
public:
    static ActiveSessions& getInstance() {
        static ActiveSessions instance;
        return instance;
    }

    void addSession(quint32 userId, ClientSession* session) {
        sessions[userId] = session;
    }

    void removeSession(quint32 userId) {
        sessions.remove(userId);
    }

    ClientSession* getSession(quint32 userId) {
        return sessions.value(userId, nullptr);
    }

private:
    ActiveSessions() {} // prywatny konstruktor dla Singleton
    QMap<quint32, ClientSession*> sessions;
};

#endif
