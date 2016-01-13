CONFIG += testcase
TARGET = tst_qdeclarativedom

QT += testlib declarative declarative-private
macx:CONFIG -= app_bundle

SOURCES += tst_qdeclarativedom.cpp

DEFINES += SRCDIR=\\\"$$PWD\\\"

CONFIG += parallel_test

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
