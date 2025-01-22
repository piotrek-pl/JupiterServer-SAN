#ifndef DATABASEQUERIES_H
#define DATABASEQUERIES_H

#include <QString>

namespace DatabaseQueries {

// Nazwy tabel
namespace Tables {
const QString USERS = "users";
const QString SESSIONS = "user_sessions";
const QString FRIENDS_PREFIX = "user_%1_friends"; // %1 będzie zastąpione przez user_id
const QString CHAT_PREFIX = "chat_%1_%2"; // %1, %2 będą ID użytkowników (mniejsze_ID_większe_ID)
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

const QString CHAT_TABLE =
    "CREATE TABLE IF NOT EXISTS %1 ("  // %1 będzie nazwą tabeli w formacie chat_X_Y
    "id INT AUTO_INCREMENT PRIMARY KEY, "
    "sender_id INT NOT NULL, "
    "message TEXT NOT NULL, "
    "sent_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, "
    "read_at TIMESTAMP NULL, "
    "FOREIGN KEY (sender_id) REFERENCES users(id)"
    ") ENGINE=InnoDB;";

const QString CHAT_INDEXES =
    "CREATE INDEX IF NOT EXISTS idx_%1_timestamp ON %1(sent_at);"
    "CREATE INDEX IF NOT EXISTS idx_%1_unread ON %1(read_at) WHERE read_at IS NULL;";
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
const QString STORE_IN_CHAT =
    "INSERT INTO %1 (sender_id, message) "  // %1 będzie nazwą tabeli chat_X_Y
    "VALUES (?, ?)";

const QString GET_CHAT_HISTORY =
    "SELECT c.id, u.username, c.message, c.sent_at, c.read_at "
    "FROM %1 c "  // %1 będzie nazwą tabeli chat_X_Y
    "INNER JOIN users u ON c.sender_id = u.id "
    "ORDER BY c.sent_at DESC "
    "LIMIT ? OFFSET ?";

const QString GET_LATEST_MESSAGES =
    "SELECT c.id, u.username, c.message, c.sent_at, c.read_at "
    "FROM %1 c "
    "INNER JOIN users u ON c.sender_id = u.id "
    "WHERE c.id <= (SELECT MAX(id) FROM %1) "  // pobierz od najwyższego ID
    "AND c.id > (SELECT MAX(id) FROM %1) - ? "  // limit określa ile wiadomości od końca
    "ORDER BY c.sent_at ASC, c.id ASC";  // sortuj rosnąco dla prawidłowej kolejności

const QString GET_MESSAGES_COUNT =
    "SELECT COUNT(*) FROM %1";  // %1 będzie nazwą tabeli chat_X_Y

const QString MARK_CHAT_READ =
    "UPDATE %1 "  // %1 będzie nazwą tabeli chat_X_Y
    "SET read_at = CURRENT_TIMESTAMP "
    "WHERE sender_id != ? AND read_at IS NULL";

const QString GET_UNREAD_COUNT =
    "SELECT COUNT(*) FROM %1 "  // %1 będzie nazwą tabeli chat_X_Y
    "WHERE sender_id != ? AND read_at IS NULL";

const QString CHECK_CHAT_TABLE_EXISTS =
    "SELECT COUNT(*) FROM information_schema.tables "
    "WHERE table_schema = DATABASE() AND table_name = ?";

const QString GET_NEW_MESSAGES =
    "SELECT m.id, u.username, m.message, m.sent_at, m.read_at, "
    "m.sender_id, m.receiver_id "
    "FROM messages m "
    "INNER JOIN users u ON m.sender_id = u.id "
    "WHERE (m.sender_id = :userId OR m.receiver_id = :userId) "
    "AND m.id > :lastId "
    "ORDER BY m.sent_at ASC, m.id ASC";
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
    "(user_id, session_token, expires_at) "
    "VALUES (?, ?, ?)";

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
