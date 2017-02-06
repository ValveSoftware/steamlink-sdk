CONFIG += testcase
TARGET = tst_qqmlapplicationengine
macx:CONFIG -= app_bundle


SOURCES += tst_qqmlapplicationengine.cpp
TESTDATA += data/*

include (../../shared/util.pri)
QT += core-private gui-private qml-private network testlib
