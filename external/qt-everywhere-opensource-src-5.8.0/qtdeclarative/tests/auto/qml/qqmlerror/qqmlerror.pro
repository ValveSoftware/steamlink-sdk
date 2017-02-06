CONFIG += testcase
TARGET = tst_qqmlerror
SOURCES += tst_qqmlerror.cpp

include (../../shared/util.pri)

macx:CONFIG -= app_bundle

TESTDATA = data/*

QT += core-private gui-private qml-private testlib
