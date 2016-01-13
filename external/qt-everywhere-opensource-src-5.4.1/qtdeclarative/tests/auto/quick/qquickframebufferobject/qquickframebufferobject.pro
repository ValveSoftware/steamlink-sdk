CONFIG += testcase
TARGET = tst_qquickframebufferobject
SOURCES += tst_qquickframebufferobject.cpp

TESTDATA = data/*
include(../../shared/util.pri)

macx:CONFIG -= app_bundle

CONFIG += parallel_test
QT += quick testlib

OTHER_FILES += \
    data/testStuff.qml

