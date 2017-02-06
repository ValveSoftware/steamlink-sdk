TARGET = tst_qquickanchors
CONFIG += testcase
SOURCES += tst_qquickanchors.cpp

include (../../shared/util.pri)
include (../shared/util.pri)

macx:CONFIG -= app_bundle

TESTDATA = data/*

QT += core-private gui-private qml-private quick-private  testlib
