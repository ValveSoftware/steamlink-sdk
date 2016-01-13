CONFIG += testcase
TARGET = tst_qqmlhangeset
macx:CONFIG -= app_bundle

SOURCES += tst_qqmlchangeset.cpp

CONFIG += parallel_test

QT += core-private gui-private qml-private testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
