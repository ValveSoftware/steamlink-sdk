CONFIG += testcase
TARGET = tst_qquickitem2
macx:CONFIG -= app_bundle

SOURCES += tst_qquickitem.cpp

include (../../shared/util.pri)

TESTDATA = data/*

QT += core-private gui-private  qml-private quick-private testlib
