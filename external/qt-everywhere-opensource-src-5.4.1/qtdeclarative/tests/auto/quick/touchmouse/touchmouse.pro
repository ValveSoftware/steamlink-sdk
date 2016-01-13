CONFIG += testcase

TARGET = tst_touchmouse
QT += core-private gui-private qml-private quick-private  testlib

macx:CONFIG -= app_bundle

SOURCES  += tst_touchmouse.cpp

include (../../shared/util.pri)
include (../shared/util.pri)

TESTDATA = data/*

# OTHER_FILES += data/foo.qml

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
