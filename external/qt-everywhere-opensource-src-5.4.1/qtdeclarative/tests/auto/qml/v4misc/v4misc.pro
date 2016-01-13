CONFIG += testcase
TARGET = tst_v4misc
macx:CONFIG -= app_bundle

SOURCES += tst_v4misc.cpp

CONFIG += parallel_test
QT += core-private qml-private testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
