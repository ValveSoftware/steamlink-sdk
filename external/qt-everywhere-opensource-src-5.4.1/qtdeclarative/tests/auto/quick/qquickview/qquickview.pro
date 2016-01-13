CONFIG += testcase
CONFIG += parallel_test
TARGET = tst_qquickview
macx:CONFIG -= app_bundle

SOURCES += tst_qquickview.cpp

include (../../shared/util.pri)

TESTDATA = data/*

QT += core-private gui-private qml-private quick-private testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
