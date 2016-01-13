CONFIG += testcase
TARGET = tst_qdeclarativeproperty

QT += testlib declarative declarative-private script-private core-private
macx:CONFIG -= app_bundle

SOURCES += tst_qdeclarativeproperty.cpp

include(../shared/qdeclarativedatatest.pri)

CONFIG += parallel_test

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
