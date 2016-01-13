CONFIG += testcase
win32-g++|mac:CONFIG+=insignificant_test # QTBUG-29062
TARGET = tst_qquickanimations
SOURCES += tst_qquickanimations.cpp

include (../../shared/util.pri)

macx:CONFIG -= app_bundle

TESTDATA = data/*

CONFIG += parallel_test

QT += core-private gui-private  qml-private quick-private testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
