CONFIG += testcase
TARGET = tst_qdeclarativepositioners

QT += testlib declarative declarative-private core-private gui-private widgets-private
SOURCES += tst_qdeclarativepositioners.cpp
macx:CONFIG -= app_bundle

DEFINES += SRCDIR=\\\"$$PWD\\\"

CONFIG += parallel_test

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
