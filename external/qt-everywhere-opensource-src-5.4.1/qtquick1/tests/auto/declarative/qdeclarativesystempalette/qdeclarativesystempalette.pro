CONFIG += testcase
TARGET = tst_qdeclarativesystempalette

QT += testlib declarative declarative-private gui widgets
macx:CONFIG -= app_bundle

SOURCES += tst_qdeclarativesystempalette.cpp

DEFINES += SRCDIR=\\\"$$PWD\\\"

CONFIG += parallel_test

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
