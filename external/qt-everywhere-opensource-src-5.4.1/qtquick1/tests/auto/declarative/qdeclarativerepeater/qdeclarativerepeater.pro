CONFIG += testcase
TARGET = tst_qdeclarativerepeater

QT += testlib declarative declarative-private gui widgets
macx:CONFIG -= app_bundle

SOURCES += tst_qdeclarativerepeater.cpp

include(../shared/qdeclarativedatatest.pri)

CONFIG += parallel_test
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
