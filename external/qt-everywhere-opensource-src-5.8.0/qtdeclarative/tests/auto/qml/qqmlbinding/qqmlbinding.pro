CONFIG += testcase
TARGET = tst_qqmlbinding
macx:CONFIG -= app_bundle

SOURCES += tst_qqmlbinding.cpp

include (../../shared/util.pri)

TESTDATA = data/*

QT += core-private gui-private qml-private quick-private testlib
