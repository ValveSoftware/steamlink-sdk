CONFIG += testcase
TARGET = tst_qv4debugger
macx:CONFIG -= app_bundle

SOURCES += tst_qv4debugger.cpp

QT += core-private gui-private qml-private network testlib
