CONFIG += testcase
TARGET = tst_qqmlimport
QT += qml testlib qml-private quick
osx:CONFIG -= app_bundle

SOURCES += tst_qqmlimport.cpp

include (../../shared/util.pri)
