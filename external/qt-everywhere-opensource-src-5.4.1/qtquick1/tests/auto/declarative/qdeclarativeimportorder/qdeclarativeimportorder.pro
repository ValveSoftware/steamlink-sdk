CONFIG += testcase
TARGET = tst_qdeclarativeimportorder

QT += testlib declarative widgets
macx:CONFIG -= app_bundle

SOURCES += tst_qdeclarativeimportorder.cpp

include(../shared/qdeclarativedatatest.pri)

CONFIG += parallel_test

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
