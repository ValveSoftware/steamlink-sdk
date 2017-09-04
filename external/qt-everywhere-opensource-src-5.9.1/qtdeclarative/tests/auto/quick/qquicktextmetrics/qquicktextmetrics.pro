CONFIG += testcase
TARGET = tst_qquicktextmetrics
osx:CONFIG -= app_bundle

SOURCES += tst_qquicktextmetrics.cpp

QT += core gui qml quick-private testlib
