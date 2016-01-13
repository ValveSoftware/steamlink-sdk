CONFIG += testcase
TARGET = tst_qmlplugindump
QT += testlib gui-private
macx:CONFIG -= app_bundle
CONFIG += parallel_test

SOURCES += tst_qmlplugindump.cpp
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
