CONFIG += testcase
TARGET = tst_qquicktextmetrics
osx:CONFIG -= app_bundle

SOURCES += tst_qquicktextmetrics.cpp

CONFIG += parallel_test

QT += core gui qml quick-private testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
