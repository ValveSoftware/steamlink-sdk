CONFIG += testcase
TARGET = tst_parserstress

QT += testlib declarative
macx:CONFIG -= app_bundle

SOURCES += tst_parserstress.cpp

DEFINES += SRCDIR=\\\"$$PWD\\\"

CONFIG += parallel_test

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
