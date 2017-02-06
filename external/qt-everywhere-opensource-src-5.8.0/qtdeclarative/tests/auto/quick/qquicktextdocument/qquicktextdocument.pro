CONFIG += testcase
TARGET = tst_qquicktextdocument
macx:CONFIG -= app_bundle

SOURCES += tst_qquicktextdocument.cpp

include (../../shared/util.pri)

TESTDATA = data/*

QT += core-private gui-private qml-private quick-private testlib

