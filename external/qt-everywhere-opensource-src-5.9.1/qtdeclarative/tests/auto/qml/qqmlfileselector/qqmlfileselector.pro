CONFIG += testcase
TARGET = tst_qqmlfileselector
macx:CONFIG -= app_bundle

SOURCES += tst_qqmlfileselector.cpp

include (../../shared/util.pri)

TESTDATA = data/*

QT += core-private gui-private qml-private testlib
