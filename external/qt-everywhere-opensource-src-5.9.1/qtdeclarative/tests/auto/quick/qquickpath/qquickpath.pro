CONFIG += testcase
TARGET = tst_qquickpath
macx:CONFIG -= app_bundle

SOURCES += tst_qquickpath.cpp

include (../../shared/util.pri)

TESTDATA = data/*

QT += core-private gui-private  qml-private quick-private testlib
