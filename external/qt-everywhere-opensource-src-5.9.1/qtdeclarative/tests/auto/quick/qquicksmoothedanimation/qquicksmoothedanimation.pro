CONFIG += testcase
TARGET = tst_qquicksmoothedanimation
macx:CONFIG -= app_bundle

SOURCES += tst_qquicksmoothedanimation.cpp

include (../../shared/util.pri)

TESTDATA = data/*

QT += core-private gui-private  qml-private quick-private testlib
