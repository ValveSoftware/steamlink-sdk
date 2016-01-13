CONFIG += testcase
TARGET = tst_qdeclarativeworkerscript

QT += testlib declarative declarative-private script core-private script-private
macx:CONFIG -= app_bundle

SOURCES += tst_qdeclarativeworkerscript.cpp

include(../shared/qdeclarativedatatest.pri)

CONFIG += parallel_test
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
