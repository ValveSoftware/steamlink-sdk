CONFIG += testcase
TARGET = tst_qqmlinfo
macx:CONFIG -= app_bundle

SOURCES += tst_qqmlinfo.cpp

include (../../shared/util.pri)

TESTDATA = data/*

QT += core-private gui-private qml-private testlib
