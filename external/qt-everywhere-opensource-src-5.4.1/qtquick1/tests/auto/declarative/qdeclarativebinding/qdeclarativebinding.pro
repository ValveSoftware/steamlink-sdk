CONFIG += testcase
TARGET = tst_qdeclarativebinding

QT += testlib declarative declarative-private widgets
macx:CONFIG -= app_bundle

SOURCES += tst_qdeclarativebinding.cpp

DEFINES += SRCDIR=\\\"$$PWD\\\"

CONFIG += parallel_test

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
