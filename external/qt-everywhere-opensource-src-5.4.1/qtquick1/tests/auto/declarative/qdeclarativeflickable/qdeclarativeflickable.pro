CONFIG += testcase
TARGET = tst_qdeclarativeflickable

QT += testlib declarative declarative-private gui widgets script-private core-private
macx:CONFIG -= app_bundle

SOURCES += tst_qdeclarativeflickable.cpp

DEFINES += SRCDIR=\\\"$$PWD\\\"

CONFIG += parallel_test
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
