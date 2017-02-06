CONFIG += testcase
TARGET = tst_qquickdynamicpropertyanimation
SOURCES += tst_qquickdynamicpropertyanimation.cpp

include (../../shared/util.pri)

macx:CONFIG -= app_bundle

TESTDATA = data/*

QT += core gui qml quick testlib
