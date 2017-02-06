CONFIG += testcase
TARGET = tst_quickfontmetrics
osx:CONFIG -= app_bundle

SOURCES += tst_quickfontmetrics.cpp

QT += core gui qml quick-private testlib
