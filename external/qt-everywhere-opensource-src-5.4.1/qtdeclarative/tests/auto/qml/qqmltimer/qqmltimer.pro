CONFIG += testcase
TARGET = tst_qqmltimer
macx:CONFIG -= app_bundle

SOURCES += tst_qqmltimer.cpp

CONFIG += parallel_test
QT += core-private gui-private qml-private quick-private gui testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
