#ifndef DATABASEQUERIES_H
#define DATABASEQUERIES_H

#include <QString>

namespace DatabaseQueries {

// Nazwy tabel
namespace Tables {
const QString USERS = "users";
const QString MESSAGES = "messages_history";
const QString SESSIONS = "user_sessions";
const QString FRIENDS_PREFIX = "user_%1_friends"; // %1 będzie zastąpione przez user_id
}

// Zapytania do tworzenia tabel
namespace Create {
const QString USERS_TABLE =
    "CREATE TABLE IF NOT EXISTS users ("
    "id INT AUTO_INCREMENT PRIMARY KEY, "
    "username VARCHAR(32) UNIQUE NOT NULL, "
    "password VARCHAR(128) NOT NULL, "
    "email VARCHAR(255), "
    "status ENUM('online', 'offline', 'away', 'busy') DEFAULT 'offline', "
    "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, "
    "last_login TIMESTAMP NULL, "
    "salt VARCHAR(32) NOT NULL"
    ") ENGINE=InnoDB;";

const QString MESSAGES_TABLE =
    "CREATE TABLE IF NOT EXISTS messages_history ("
    "id INT AUTO_INCREMENT PRIMARY KEY, "
    "sender_id INT NOT NULL, "
    "receiver_id INT NOT NULL, "
    "message TEXT NOT NULL, "
    "sent_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, "
    "read_at TIMESTAMP NULL, "
    "deleted_at TIMESTAMP NULL, "
    "deleted_by INT, "
    "FOREIGN KEY (sender_id) REFERENCES users(id), "
    "FOREIGN KEY (receiver_id) REFERENCES users(id), "
    "FOREIGN KEY (deleted_by) REFERENCES users(id)"
    ") ENGINE=InnoDB;";

const QString SESSIONS_TABLE =
    "CREATE TABLE IF NOT EXISTS user_sessions ("
    "id INT AUTO_INCREMENT PRIMARY KEY, "
    "user_id INT NOT NULL, "
    "session_token VARCHAR(64) NOT NULL, "
    "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, "
    "expires_at TIMESTAMP NULL, "
    "FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE"
    ") ENGINE=InnoDB;";

const QString FRIENDS_TABLE =
    "CREATE TABLE IF NOT EXISTS user_%1_friends ("
    "friend_id INT NOT NULL, "
    "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, "
    "FOREIGN KEY (friend_id) REFERENCES users(id)"
    ") ENGINE=InnoDB;";
}

// Zapytania związane z użytkownikami
namespace Users {
// Autoryzacja i rejestracja
const QString AUTHENTICATE =
    "SELECT id, password, salt FROM users WHERE username = ?";

const QString REGISTER =
    "INSERT INTO users (username, password, salt, status) "
    "VALUES (?, ?, ?, 'offline')";

// Zarządzanie statusem
const QString UPDATE_STATUS =
    "UPDATE users SET status = ?, last_login = CURRENT_TIMESTAMP "
    "WHERE id = ?";

const QString GET_STATUS =
    "SELECT status FROM users WHERE id = ?";

// Sprawdzanie istnienia
const QString EXISTS_BY_NAME =
    "SELECT COUNT(*) FROM users WHERE username = ?";

const QString EXISTS_BY_ID =
    "SELECT COUNT(*) FROM users WHERE id = ?";

// Pobieranie informacji
const QString GET_USERNAME =
    "SELECT username FROM users WHERE id = ?";

const QString GET_USER_INFO =
    "SELECT username, email, status, created_at, last_login "
    "FROM users WHERE id = ?";

const QString UPDATE_LAST_LOGIN =
    "UPDATE users SET last_login = CURRENT_TIMESTAMP "
    "WHERE id = ?";
}

// Zapytania związane z wiadomościami
namespace Messages {
const QString STORE =
    "INSERT INTO messages_history (sender_id, receiver_id, message) "
    "VALUES (?, ?, ?)";

const QString GET_HISTORY =
    "SELECT m.id, u.username, m.message, m.sent_at, m.read_at "
    "FROM messages_history m "
    "INNER JOIN users u ON m.sender_id = u.id "
    "WHERE ((m.sender_id = ? AND m.receiver_id = ?) "
    "    OR (m.sender_id = ? AND m.receiver_id = ?)) "
    "    AND m.deleted_at IS NULL "
    "ORDER BY m.sent_at DESC LIMIT ?";

const QString MARK_READ =
    "UPDATE messages_history "
    "SET read_at = CURRENT_TIMESTAMP "
    "WHERE id = ? AND (sender_id = ? OR receiver_id = ?) "
    "AND read_at IS NULL";

const QString DELETE =
    "UPDATE messages_history "
    "SET deleted_at = CURRENT_TIMESTAMP, deleted_by = ? "
    "WHERE id = ? AND (sender_id = ? OR receiver_id = ?)";

const QString GET_UNREAD_COUNT =
    "SELECT COUNT(*) FROM messages_history "
    "WHERE receiver_id = ? AND read_at IS NULL "
    "AND deleted_at IS NULL";
}

// Zapytania związane ze znajomymi
namespace Friends {
const QString LIST =
    "SELECT u.id, u.username, u.status "
    "FROM users u "
    "INNER JOIN user_%1_friends f ON f.friend_id = u.id "
    "ORDER BY u.status, u.username";

const QString ADD =
    "INSERT INTO user_%1_friends (friend_id) VALUES (?)";

const QString REMOVE =
    "DELETE FROM user_%1_friends WHERE friend_id = ?";

const QString CHECK =
    "SELECT COUNT(*) FROM user_%1_friends "
    "WHERE friend_id = ?";

const QString GET_ONLINE =
    "SELECT u.id, u.username "
    "FROM users u "
    "INNER JOIN user_%1_friends f ON f.friend_id = u.id "
    "WHERE u.status = 'online'";
}

// Zapytania związane z sesjami
namespace Sessions {
const QString CREATE =
    "INSERT INTO user_sessions "
    "(user_id, session_token, ip_address, expires_at) "
    "VALUES (?, ?, ?, ?)";

const QString UPDATE =
    "UPDATE user_sessions "
    "SET last_activity = CURRENT_TIMESTAMP "
    "WHERE session_token = ?";

const QString VALIDATE =
    "SELECT user_id FROM user_sessions "
    "WHERE session_token = ? "
    "AND expires_at > CURRENT_TIMESTAMP";

const QString CLEANUP =
    "DELETE FROM user_sessions "
    "WHERE expires_at < CURRENT_TIMESTAMP";
}

} // namespace DatabaseQueries

#endif // DATABASEQUERIES_H
