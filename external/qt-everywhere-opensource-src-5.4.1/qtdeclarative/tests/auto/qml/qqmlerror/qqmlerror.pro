CONFIG += testcase
TARGET = tst_qqmlerror
SOURCES += tst_qqmlerror.cpp

include (../../shared/util.pri)

macx:CONFIG -= app_bundle

CONFIG += parallel_test

TESTDATA = data/*

QT += core-private gui-private qml-private testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
