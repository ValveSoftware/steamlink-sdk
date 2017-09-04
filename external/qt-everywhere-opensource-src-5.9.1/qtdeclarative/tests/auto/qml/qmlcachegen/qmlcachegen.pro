CONFIG += testcase
TARGET = tst_qmlcachegen
macos:CONFIG -= app_bundle

SOURCES += tst_qmlcachegen.cpp

QT += core-private qml-private testlib
