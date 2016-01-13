CONFIG += testcase
TARGET = tst_qqmlfileselector
macx:CONFIG -= app_bundle

SOURCES += tst_qqmlfileselector.cpp

include (../../shared/util.pri)

TESTDATA = data/*

CONFIG += parallel_test
QT += core-private gui-private qml-private testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
