CONFIG += testcase
TARGET = tst_qquickview
macx:CONFIG -= app_bundle

SOURCES += tst_qquickview.cpp

include (../../shared/util.pri)

TESTDATA = data/*

QT += core-private gui-private qml-private quick-private testlib
