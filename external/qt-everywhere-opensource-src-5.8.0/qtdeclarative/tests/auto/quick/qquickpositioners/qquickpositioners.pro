CONFIG += testcase
TARGET = tst_qquickpositioners
SOURCES += tst_qquickpositioners.cpp

include (../shared/util.pri)
include (../../shared/util.pri)

macx:CONFIG -= app_bundle

TESTDATA = data/*

QT += testlib
