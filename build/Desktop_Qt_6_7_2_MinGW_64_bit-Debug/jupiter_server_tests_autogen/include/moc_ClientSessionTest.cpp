/****************************************************************************
** Meta object code from reading C++ file 'ClientSessionTest.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.7.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../tests/ClientSessionTest.h"
#include <QtNetwork/QSslError>
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'ClientSessionTest.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 68
#error "This file was generated using the moc from 6.7.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {

#ifdef QT_MOC_HAS_STRINGDATA
struct qt_meta_stringdata_CLASSClientSessionTestENDCLASS_t {};
constexpr auto qt_meta_stringdata_CLASSClientSessionTestENDCLASS = QtMocHelpers::stringData(
    "ClientSessionTest",
    "initTestCase",
    "",
    "cleanupTestCase",
    "init",
    "cleanup",
    "testConnectionInitialization",
    "testAuthentication",
    "testMessageHandling",
    "testPingPongMechanism",
    "testStatusUpdate",
    "testMessageAcknowledgement"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_CLASSClientSessionTestENDCLASS[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
      10,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,   74,    2, 0x08,    1 /* Private */,
       3,    0,   75,    2, 0x08,    2 /* Private */,
       4,    0,   76,    2, 0x08,    3 /* Private */,
       5,    0,   77,    2, 0x08,    4 /* Private */,
       6,    0,   78,    2, 0x08,    5 /* Private */,
       7,    0,   79,    2, 0x08,    6 /* Private */,
       8,    0,   80,    2, 0x08,    7 /* Private */,
       9,    0,   81,    2, 0x08,    8 /* Private */,
      10,    0,   82,    2, 0x08,    9 /* Private */,
      11,    0,   83,    2, 0x08,   10 /* Private */,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

Q_CONSTINIT const QMetaObject ClientSessionTest::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_CLASSClientSessionTestENDCLASS.offsetsAndSizes,
    qt_meta_data_CLASSClientSessionTestENDCLASS,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_CLASSClientSessionTestENDCLASS_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<ClientSessionTest, std::true_type>,
        // method 'initTestCase'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'cleanupTestCase'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'init'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'cleanup'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'testConnectionInitialization'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'testAuthentication'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'testMessageHandling'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'testPingPongMechanism'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'testStatusUpdate'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'testMessageAcknowledgement'
        QtPrivate::TypeAndForceComplete<void, std::false_type>
    >,
    nullptr
} };

void ClientSessionTest::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<ClientSessionTest *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->initTestCase(); break;
        case 1: _t->cleanupTestCase(); break;
        case 2: _t->init(); break;
        case 3: _t->cleanup(); break;
        case 4: _t->testConnectionInitialization(); break;
        case 5: _t->testAuthentication(); break;
        case 6: _t->testMessageHandling(); break;
        case 7: _t->testPingPongMechanism(); break;
        case 8: _t->testStatusUpdate(); break;
        case 9: _t->testMessageAcknowledgement(); break;
        default: ;
        }
    }
    (void)_a;
}

const QMetaObject *ClientSessionTest::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ClientSessionTest::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_CLASSClientSessionTestENDCLASS.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int ClientSessionTest::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 10)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 10;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 10)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 10;
    }
    return _id;
}
QT_WARNING_POP
