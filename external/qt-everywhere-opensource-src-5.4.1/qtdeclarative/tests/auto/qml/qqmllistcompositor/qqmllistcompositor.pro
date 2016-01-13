CONFIG += testcase
TARGET = tst_qqmllistcompositor
macx:CONFIG -= app_bundle

SOURCES += tst_qqmllistcompositor.cpp

CONFIG += parallel_test

QT += core-private gui-private qml-private quick-private testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
