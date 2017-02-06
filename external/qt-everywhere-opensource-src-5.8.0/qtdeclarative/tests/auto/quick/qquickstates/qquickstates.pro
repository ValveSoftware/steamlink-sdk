CONFIG += testcase
TARGET = tst_qquickstates
macx:CONFIG -= app_bundle

SOURCES += tst_qquickstates.cpp

include (../../shared/util.pri)

TESTDATA = data/*

QT += core-private gui-private  qml-private quick-private testlib
