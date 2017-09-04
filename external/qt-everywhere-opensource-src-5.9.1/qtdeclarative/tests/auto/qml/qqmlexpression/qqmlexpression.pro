CONFIG += testcase
TARGET = tst_qqmlexpression
macx:CONFIG -= app_bundle

SOURCES += tst_qqmlexpression.cpp

include (../../shared/util.pri)

TESTDATA = data/*

QT += core-private gui-private qml-private testlib
