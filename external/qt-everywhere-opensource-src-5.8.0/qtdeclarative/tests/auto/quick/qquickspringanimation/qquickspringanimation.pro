CONFIG += testcase
TARGET = tst_qquickspringanimation
macx:CONFIG -= app_bundle

SOURCES += tst_qquickspringanimation.cpp

include (../../shared/util.pri)

TESTDATA = data/*

QT += core-private gui-private  qml-private quick-private testlib
