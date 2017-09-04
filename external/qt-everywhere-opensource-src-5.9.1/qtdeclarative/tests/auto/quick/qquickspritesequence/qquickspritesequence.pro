CONFIG += testcase
TARGET = tst_qquickspritesequence
SOURCES += tst_qquickspritesequence.cpp

include (../../shared/util.pri)

macx:CONFIG -= app_bundle

TESTDATA = data/*

QT += core-private gui-private qml-private quick-private network testlib
