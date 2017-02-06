CONFIG += testcase
TARGET = tst_qqmlproperty
macx:CONFIG -= app_bundle

SOURCES += tst_qqmlproperty.cpp

include (../../shared/util.pri)

TESTDATA = data/*

QT += core-private gui-private  qml-private testlib
