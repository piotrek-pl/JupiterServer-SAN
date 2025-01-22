# Określamy minimalną wersję CMake
cmake_minimum_required(VERSION 3.16)

# Definiujemy projekt
project(JupiterServer
    VERSION 2.0
    DESCRIPTION "Jupiter Messenger Server Application"
    LANGUAGES CXX
)

# Ustawienia kompilatora i Qt
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Szukamy Qt 6.7.2
find_package(Qt6 6.7.2 COMPONENTS
    Core
    Network
    Sql
    WebSockets
    Test    # Dodajemy komponent Test
    REQUIRED
)

# Opcja włączenia testów (domyślnie włączone)
option(BUILD_TESTS "Build tests" ON)

# Definiujemy pliki źródłowe
set(PROJECT_SOURCES
    src/main.cpp
    src/server/Server.cpp
    src/server/ClientSession.cpp
    src/server/Protocol.cpp
    src/database/DatabaseManager.cpp
    src/database/DatabaseQueries.cpp
    src/models/User.cpp
    src/models/Message.cpp
    src/network/NotificationManager.cpp
)

# Definiujemy pliki nagłówkowe
set(PROJECT_HEADERS
    src/server/Server.h
    src/server/ClientSession.h
    src/server/Protocol.h
    src/database/DatabaseManager.h
    src/database/DatabaseQueries.h
    src/models/User.h
    src/models/Message.h
    src/network/NotificationManager.h
)

# Konfiguracja plików zasobów
set(CONFIG_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/config/database.conf
    ${CMAKE_CURRENT_SOURCE_DIR}/config/databaseTest.conf
    ${CMAKE_CURRENT_SOURCE_DIR}/scripts/initDatabase.sql
)

# Sprawdzanie obecności plików konfiguracyjnych
foreach(CONFIG_FILE ${CONFIG_FILES})
    if(NOT EXISTS ${CONFIG_FILE})
        message(FATAL_ERROR "Nie znaleziono pliku: ${CONFIG_FILE}")
    endif()
endforeach()

# Kopiowanie plików konfiguracyjnych do katalogu build
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/config/database.conf
    ${CMAKE_BINARY_DIR}/config/database.conf
    COPYONLY
)

# Tworzenie katalogu scripts i kopiowanie pliku SQL
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/scripts)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/scripts/initDatabase.sql
    ${CMAKE_BINARY_DIR}/scripts/initDatabase.sql
    COPYONLY
)

# Tworzymy główny executable
add_executable(${PROJECT_NAME}
    ${PROJECT_SOURCES}
    ${PROJECT_HEADERS}
    src/server/ActiveSessions.h

)

# Dodajemy ścieżki include
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Linkujemy z Qt
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Sql
    Qt6::WebSockets
)

# Sekcja testów
if(BUILD_TESTS)
    enable_testing()

    # Definiujemy pliki źródłowe i nagłówkowe testów
    set(TEST_SOURCES
        tests/main_test.cpp
        tests/ProtocolTest.cpp
        tests/ClientSessionTest.cpp
        tests/TestSocket.cpp
        tests/TestDatabaseQueries.cpp
        # Dodajemy potrzebne pliki źródłowe z projektu
        src/server/Protocol.cpp
        src/server/ClientSession.cpp
        src/database/DatabaseManager.cpp
    )

    set(TEST_HEADERS
        tests/ProtocolTest.h
        tests/ClientSessionTest.h
        tests/TestSocket.h
        tests/TestDatabaseQueries.h
    )

    # Kopiowanie plików konfiguracyjnych dla testów
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/config/databaseTest.conf
        ${CMAKE_BINARY_DIR}/config/databaseTest.conf
        COPYONLY
    )

    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/tests/scripts)
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/initDatabase.sql
        ${CMAKE_BINARY_DIR}/tests/scripts/initDatabase.sql
        COPYONLY
    )

    # Tworzymy executable dla testów
    add_executable(jupiter_server_tests
        ${TEST_SOURCES}
        ${TEST_HEADERS}  # Dodajemy pliki nagłówkowe
    )

    # Linkujemy testy z odpowiednimi bibliotekami
    target_link_libraries(jupiter_server_tests PRIVATE
        Qt6::Core
        Qt6::Network
        Qt6::Sql
        Qt6::Test
        Qt6::WebSockets
    )

    # Dodajemy ścieżki include dla testów
    target_include_directories(jupiter_server_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/tests
        ${CMAKE_CURRENT_BINARY_DIR}  # Dla plików generowanych przez MOC
    )

    # Dodajemy test do CTest
    add_test(NAME jupiter_server_tests COMMAND jupiter_server_tests)
endif()
