CONFIG += testcase
TARGET = tst_qqmlparser
QT += qml-private testlib
macx:CONFIG -= app_bundle

SOURCES += tst_qqmlparser.cpp
DEFINES += SRCDIR=\\\"$$PWD\\\"

CONFIG += parallel_test

cross_compile: DEFINES += QTEST_CROSS_COMPILED
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
