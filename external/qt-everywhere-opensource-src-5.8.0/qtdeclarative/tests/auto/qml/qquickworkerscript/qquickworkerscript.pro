CONFIG += testcase
TARGET = tst_qquickworkerscript
macx:CONFIG -= app_bundle

SOURCES += tst_qquickworkerscript.cpp

include (../../shared/util.pri)

TESTDATA = data/*

QT += core-private gui-private  qml-private testlib
