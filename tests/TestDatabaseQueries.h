#ifndef TESTDATABASEQUERIES_H
#define TESTDATABASEQUERIES_H

#include <QString>

namespace TestDatabaseQueries {

namespace Config {
const QString CONFIG_FILE = "config/databaseTest.conf";
}

namespace Setup {
// Wyłączenie/włączenie kluczy obcych
const QString DISABLE_FOREIGN_KEYS = "SET FOREIGN_KEY_CHECKS = 0";
const QString ENABLE_FOREIGN_KEYS = "SET FOREIGN_KEY_CHECKS = 1";

// Usuwanie tabel
const QString DROP_MESSAGES_TABLE = "DROP TABLE IF EXISTS messages";
const QString DROP_USER_SESSIONS_TABLE = "DROP TABLE IF EXISTS user_sessions";
const QString DROP_USERS_TABLE = "DROP TABLE IF EXISTS users";
}

namespace Verify {
// Te zapytania są używane w testach do weryfikacji stanu
const QString GET_USER_STATUS = "SELECT status FROM users WHERE id = ?";
}

} // namespace TestDatabaseQueries

#endif // TESTDATABASEQUERIES_H
