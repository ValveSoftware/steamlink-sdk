CONFIG += testcase
TARGET = tst_qdeclarativeinfo

QT += testlib declarative widgets
macx:CONFIG -= app_bundle

SOURCES += tst_qdeclarativeinfo.cpp

include(../shared/qdeclarativedatatest.pri)

CONFIG += parallel_test

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
