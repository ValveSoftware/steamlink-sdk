CONFIG += testcase
TARGET = tst_qqmllistmodel
macx:CONFIG -= app_bundle

SOURCES += tst_qqmllistmodel.cpp

include (../../shared/util.pri)

TESTDATA = data/*

QT += core-private gui-private  qml-private quick-private testlib
