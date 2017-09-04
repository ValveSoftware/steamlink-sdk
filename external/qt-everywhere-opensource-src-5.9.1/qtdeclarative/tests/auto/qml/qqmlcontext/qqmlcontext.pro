CONFIG += testcase
TARGET = tst_qqmlcontext
SOURCES += tst_qqmlcontext.cpp

include (../../shared/util.pri)

macx:CONFIG -= app_bundle

TESTDATA = data/*

QT += core-private gui-private qml-private testlib
