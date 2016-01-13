CONFIG += testcase
TARGET = tst_qdeclarativestates

QT += testlib declarative declarative-private core-private script-private widgets-private gui-private
macx:CONFIG -= app_bundle

SOURCES += tst_qdeclarativestates.cpp

DEFINES += SRCDIR=\\\"$$PWD\\\"

CONFIG += parallel_test
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
