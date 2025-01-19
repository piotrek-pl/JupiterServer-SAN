find_package(Qt6 COMPONENTS Test REQUIRED)

set(TEST_SOURCES
    ProtocolTest.cpp
    ClientSessionTest.cpp
    DatabaseTest.cpp
    main_test.cpp
)

add_executable(jupiter_server_tests ${TEST_SOURCES})

target_link_libraries(jupiter_server_tests PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Sql
    Qt6::Test
)

# Dodajemy ścieżki include
target_include_directories(jupiter_server_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

# Dodajemy testy do CTest
add_test(NAME jupiter_server_tests COMMAND jupiter_server_tests)