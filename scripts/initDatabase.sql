-- Tabela użytkowników
CREATE TABLE IF NOT EXISTS users (
    id INT UNSIGNED NOT NULL AUTO_INCREMENT,
    username VARCHAR(50) NOT NULL UNIQUE,
    password CHAR(64) NOT NULL, -- SHA-256 hash
    email VARCHAR(100),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    last_login TIMESTAMP NULL,
    status ENUM('offline', 'online', 'away', 'busy') DEFAULT 'offline',
    PRIMARY KEY (id)
) ENGINE=InnoDB;

-- Tabela zaproszeń do znajomych
CREATE TABLE IF NOT EXISTS friend_requests (
    id INT UNSIGNED NOT NULL AUTO_INCREMENT,
    sender_id INT UNSIGNED NOT NULL,
    receiver_id INT UNSIGNED NOT NULL,
    status ENUM('pending', 'accepted', 'rejected') DEFAULT 'pending',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP NULL ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    UNIQUE KEY unique_request (sender_id, receiver_id),
    FOREIGN KEY (sender_id) REFERENCES users(id) ON DELETE CASCADE,
    FOREIGN KEY (receiver_id) REFERENCES users(id) ON DELETE CASCADE
) ENGINE=InnoDB;

-- Tabela globalnych wiadomości (historia)
CREATE TABLE IF NOT EXISTS messages_history (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    sender_id INT UNSIGNED NOT NULL,
    receiver_id INT UNSIGNED NOT NULL,
    message TEXT NOT NULL,
    sent_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    read_at TIMESTAMP NULL,
    deleted BOOLEAN DEFAULT FALSE,
    PRIMARY KEY (id),
    FOREIGN KEY (sender_id) REFERENCES users(id) ON DELETE CASCADE,
    FOREIGN KEY (receiver_id) REFERENCES users(id) ON DELETE CASCADE,
    INDEX idx_sender_receiver (sender_id, receiver_id),
    INDEX idx_sent_at (sent_at)
) ENGINE=InnoDB;

-- Tabela sesji użytkowników
CREATE TABLE IF NOT EXISTS user_sessions (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    user_id INT UNSIGNED NOT NULL,
    session_token CHAR(64) NOT NULL,
    ip_address VARCHAR(45) NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    expires_at TIMESTAMP NOT NULL,
    last_activity TIMESTAMP NULL,
    PRIMARY KEY (id),
    UNIQUE KEY unique_session_token (session_token),
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,
    INDEX idx_expires_at (expires_at)
) ENGINE=InnoDB;

-- Procedura do tworzenia tabeli znajomych dla nowego użytkownika
DELIMITER //
CREATE PROCEDURE IF NOT EXISTS create_user_friends_table(IN user_id INT UNSIGNED)
BEGIN
    SET @sql = CONCAT('CREATE TABLE IF NOT EXISTS user_', user_id, '_friends (
        friend_id INT UNSIGNED NOT NULL,
        added_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
        nickname VARCHAR(50),
        FOREIGN KEY (friend_id) REFERENCES users(id) ON DELETE CASCADE,
        PRIMARY KEY (friend_id)
    ) ENGINE=InnoDB');
    PREPARE stmt FROM @sql;
    EXECUTE stmt;
    DEALLOCATE PREPARE stmt;
END //
DELIMITER ;

-- Trigger do automatycznego tworzenia tabeli znajomych po rejestracji użytkownika
DELIMITER //
CREATE TRIGGER IF NOT EXISTS after_user_creation
AFTER INSERT ON users
FOR EACH ROW
BEGIN
    CALL create_user_friends_table(NEW.id);
END //
DELIMITER ;

-- Indeksy
CREATE INDEX IF NOT EXISTS idx_username ON users(username);
CREATE INDEX IF NOT EXISTS idx_status ON users(status);
CREATE INDEX IF NOT EXISTS idx_friend_requests_status ON friend_requests(status);
