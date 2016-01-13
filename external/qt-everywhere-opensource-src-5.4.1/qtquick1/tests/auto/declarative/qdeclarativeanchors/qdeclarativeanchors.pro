CONFIG += testcase
TARGET = tst_qdeclarativeanchors

QT += testlib declarative declarative-private widgets widgets-private gui gui-private core-private
SOURCES += tst_qdeclarativeanchors.cpp
macx:CONFIG -= app_bundle

include(../shared/qdeclarativedatatest.pri)

CONFIG += parallel_test

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
