CONFIG += testcase
TARGET = tst_qtqmlmodules
SOURCES += tst_qtqmlmodules.cpp

include (../../shared/util.pri)

macx:CONFIG -= app_bundle

TESTDATA = data/*

QT += core-private  qml-private testlib gui gui-private
