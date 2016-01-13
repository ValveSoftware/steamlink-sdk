CONFIG += testcase
TARGET = tst_qqmlbundle
macx:CONFIG -= app_bundle

SOURCES += tst_qqmlbundle.cpp
HEADERS +=

include (../../shared/util.pri)

TESTDATA = data/*

CONFIG += parallel_test

QT += qml-private testlib gui-private

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
