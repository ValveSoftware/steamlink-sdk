CONFIG += testcase
TARGET = tst_qqmlcpputils
macx:CONFIG -= app_bundle

SOURCES += tst_qqmlcpputils.cpp

CONFIG += parallel_test

QT += core-private gui-private qml-private testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
