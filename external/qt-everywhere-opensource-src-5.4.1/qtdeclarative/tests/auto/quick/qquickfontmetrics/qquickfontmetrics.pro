CONFIG += testcase
TARGET = tst_quickfontmetrics
osx:CONFIG -= app_bundle

SOURCES += tst_quickfontmetrics.cpp

CONFIG += parallel_test

QT += core gui qml quick-private testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
