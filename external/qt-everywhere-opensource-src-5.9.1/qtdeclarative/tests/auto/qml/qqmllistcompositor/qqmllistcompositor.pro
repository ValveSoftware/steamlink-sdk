CONFIG += testcase
TARGET = tst_qqmllistcompositor
macx:CONFIG -= app_bundle

SOURCES += tst_qqmllistcompositor.cpp

QT += core-private gui-private qml-private quick-private testlib
