CONFIG += testcase
TARGET = tst_qquickdynamicpropertyanimation
SOURCES += tst_qquickdynamicpropertyanimation.cpp

include (../../shared/util.pri)

macx:CONFIG -= app_bundle

TESTDATA = data/*

CONFIG += parallel_test

QT += core gui qml quick testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
