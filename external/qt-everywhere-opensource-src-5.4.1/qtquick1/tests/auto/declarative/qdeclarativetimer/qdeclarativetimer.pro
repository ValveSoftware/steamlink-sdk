CONFIG += testcase
TARGET = tst_qdeclarativetimer

QT += testlib declarative declarative-private gui core-private
macx:CONFIG -= app_bundle

SOURCES += tst_qdeclarativetimer.cpp

DEFINES += SRCDIR=\\\"$$PWD\\\"

CONFIG += parallel_test
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
