CONFIG += testcase
TARGET = tst_qqmlmetaobject
macx:CONFIG -= app_bundle

SOURCES += tst_qqmlmetaobject.cpp

include (../../shared/util.pri)

TESTDATA = data/*

CONFIG += parallel_test
QT += qml testlib gui-private
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
