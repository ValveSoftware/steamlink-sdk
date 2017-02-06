TEMPLATE = app
TARGET = tst_qsensorgesturepluginstest
!no_system_tests:CONFIG += testcase

QT += core testlib sensors
QT -= gui

SOURCES += tst_qsensorgesturepluginstest.cpp

VPATH += ../qsensor
INCLUDEPATH += ../qsensor

HEADERS += \
    test_backends.h

SOURCES += \
    test_backends.cpp
