CONFIG += testcase
TARGET = tst_qdeclarativecontext

QT += testlib declarative
SOURCES += tst_qdeclarativecontext.cpp
macx:CONFIG -= app_bundle

DEFINES += SRCDIR=\\\"$$PWD\\\"

CONFIG += parallel_test

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
