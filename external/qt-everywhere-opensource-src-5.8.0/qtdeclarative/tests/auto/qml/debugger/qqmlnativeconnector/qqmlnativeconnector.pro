CONFIG += testcase qml_debug
TARGET = tst_qqmlnativeconnector
QT += qml testlib gui-private core-private
osx:CONFIG -= app_bundle

SOURCES +=     tst_qqmlnativeconnector.cpp
