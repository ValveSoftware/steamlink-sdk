CONFIG += testcase
TARGET = tst_qqmlhangeset
macx:CONFIG -= app_bundle

SOURCES += tst_qqmlchangeset.cpp

QT += core-private gui-private qml-private testlib
