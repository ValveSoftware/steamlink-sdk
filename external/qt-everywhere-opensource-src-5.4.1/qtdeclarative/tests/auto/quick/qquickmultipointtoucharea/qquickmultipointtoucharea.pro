TARGET = tst_qquickmultipointtoucharea
CONFIG += testcase
CONFIG += parallel_test
macx:CONFIG -= app_bundle

SOURCES += tst_qquickmultipointtoucharea.cpp

TESTDATA = data/*

include(../../shared/util.pri)
include(../shared/util.pri)

QT += core-private gui-private qml-private quick-private testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
