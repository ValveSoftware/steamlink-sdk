CONFIG += testcase
TARGET = tst_qquickdesignersupport
SOURCES += tst_qquickdesignersupport.cpp

include (../../shared/util.pri)
include (../shared/util.pri)

osx:CONFIG -= app_bundle

TESTDATA = data/*

QT += core-private gui-private qml-private quick-private testlib

DISTFILES += \
    data/TestComponent.qml
