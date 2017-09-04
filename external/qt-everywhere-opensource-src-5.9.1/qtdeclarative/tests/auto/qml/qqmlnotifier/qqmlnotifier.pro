CONFIG += testcase
TARGET = tst_qqmlnotifier
SOURCES += tst_qqmlnotifier.cpp

include (../../shared/util.pri)

macx:CONFIG -= app_bundle

TESTDATA = data/*

QT += qml testlib
