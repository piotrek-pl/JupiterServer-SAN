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
    REQUIRED
)

# Definiujemy pliki źródłowe
set(PROJECT_SOURCES
    src/main.cpp
    src/server/Server.cpp
    src/server/ClientSession.cpp
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
    src/database/DatabaseManager.h
    src/database/DatabaseQueries.h
    src/models/User.h
    src/models/Message.h
    src/network/NotificationManager.h
)

# Tworzymy executable
add_executable(${PROJECT_NAME}
    ${PROJECT_SOURCES}
    ${PROJECT_HEADERS}
    ${PROJECT_RESOURCES}
)

# Dodajemy ścieżki include
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
)

# Linkujemy z Qt
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Sql
    Qt6::WebSockets
)

# Ustawiamy opcje kompilatora
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /W4
        /WX
    )
else()
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )
endif()

# Definiujemy konfigurację Debug/Release
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNDEBUG")

# Kopiujemy pliki konfiguracyjne
set(CONFIG_FILES
    config/database.conf
    scripts/initDatabase.sql
)

foreach(CONFIG_FILE ${CONFIG_FILES})
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/${CONFIG_FILE}
        ${CMAKE_CURRENT_BINARY_DIR}/${CONFIG_FILE}
        COPYONLY
    )
endforeach()

# Instalacja
include(GNUInstallDirs)

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/config/databaseConfig.conf
    DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/${PROJECT_NAME}
)

install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/scripts/initDatabase.sql
    DESTINATION ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/sql
)

# Dodajemy opcję włączenia testów (jeśli będą potrzebne w przyszłości)
option(BUILD_TESTS "Build tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Wyświetl podsumowanie konfiguracji
message(STATUS "")
message(STATUS "Generated ${PROJECT_NAME} with config:")
message(STATUS "  Qt version: ${Qt6_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "")
