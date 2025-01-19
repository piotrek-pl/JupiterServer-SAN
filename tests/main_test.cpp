/**
 * @file main_test.cpp
 * @brief Main test runner
 * @author piotrek-pl
 * @date 2025-01-19 19:57:30
 */

#include <QCoreApplication>
#include <QTest>
#include "ProtocolTest.h"
#include "ClientSessionTest.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv); // Dodajemy tę linię

    int status = 0;

    {
        ProtocolTest tc;
        status |= QTest::qExec(&tc, argc, argv);
    }

    {
        ClientSessionTest tc;
        status |= QTest::qExec(&tc, argc, argv);
    }

    return status;
}
