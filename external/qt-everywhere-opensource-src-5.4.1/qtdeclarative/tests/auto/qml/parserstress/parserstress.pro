CONFIG += testcase
TARGET = tst_parserstress
macx:CONFIG -= app_bundle

SOURCES += tst_parserstress.cpp

TESTDATA = tests/*

CONFIG += parallel_test

QT += core-private gui-private qml-private testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
