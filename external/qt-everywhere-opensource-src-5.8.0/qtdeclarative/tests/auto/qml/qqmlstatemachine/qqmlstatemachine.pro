CONFIG += testcase
TARGET = tst_qqmlstatemachine
osx:CONFIG -= app_bundle

SOURCES += tst_qqmlstatemachine.cpp

include (../../shared/util.pri)

QT += core-private gui-private qml-private quick-private gui testlib
