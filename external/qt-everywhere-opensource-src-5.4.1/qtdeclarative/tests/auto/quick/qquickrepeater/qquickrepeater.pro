CONFIG += testcase
TARGET = tst_qquickrepeater
macx:CONFIG -= app_bundle

SOURCES += tst_qquickrepeater.cpp

include (../../shared/util.pri)
include (../shared/util.pri)

TESTDATA = data/*

CONFIG += parallel_test
QT += core-private gui-private qml-private quick-private testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
