CONFIG += testcase
TARGET = tst_qdeclarativefontloader

QT += testlib declarative declarative-private gui network
macx:CONFIG -= app_bundle

HEADERS += ../shared/testhttpserver.h
SOURCES += tst_qdeclarativefontloader.cpp ../shared/testhttpserver.cpp
include(../shared/qdeclarativedatatest.pri)
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
