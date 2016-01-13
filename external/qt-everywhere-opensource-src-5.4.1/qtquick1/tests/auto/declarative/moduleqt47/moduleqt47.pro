CONFIG += testcase
TARGET = tst_moduleqt47

QT += testlib declarative gui widgets
macx:CONFIG -= app_bundle

SOURCES += tst_moduleqt47.cpp

DEFINES += SRCDIR=\\\"$$PWD\\\"

CONFIG += parallel_test

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
