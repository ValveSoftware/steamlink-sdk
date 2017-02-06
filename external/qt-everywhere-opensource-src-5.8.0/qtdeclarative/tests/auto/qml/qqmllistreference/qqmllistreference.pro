CONFIG += testcase
TARGET = tst_qqmllistreference
macx:CONFIG -= app_bundle

SOURCES += tst_qqmllistreference.cpp

include (../../shared/util.pri)

TESTDATA = data/*

QT += core-private gui-private qml-private testlib
