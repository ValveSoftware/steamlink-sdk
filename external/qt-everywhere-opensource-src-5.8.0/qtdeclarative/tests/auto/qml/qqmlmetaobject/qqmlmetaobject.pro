CONFIG += testcase
TARGET = tst_qqmlmetaobject
macx:CONFIG -= app_bundle

SOURCES += tst_qqmlmetaobject.cpp

include (../../shared/util.pri)

TESTDATA = data/*

QT += qml testlib gui-private
