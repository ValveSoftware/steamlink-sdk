CONFIG += testcase
TARGET = tst_qqmlsettings
macx:CONFIG -= app_bundle

SOURCES += tst_qqmlsettings.cpp

include (../../shared/util.pri)

TESTDATA = data/*

QT += qml testlib
CONFIG += parallel_test
