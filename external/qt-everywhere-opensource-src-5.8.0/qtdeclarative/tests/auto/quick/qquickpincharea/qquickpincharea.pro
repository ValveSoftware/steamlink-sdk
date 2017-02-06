CONFIG += testcase
TARGET = tst_qquickpincharea
macx:CONFIG -= app_bundle

SOURCES += tst_qquickpincharea.cpp

include (../../shared/util.pri)
include (../shared/util.pri)

TESTDATA = data/*

QT += core-private gui-private qml-private quick-private testlib
