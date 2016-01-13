CONFIG += testcase
TARGET = tst_qquickopenglinfo
SOURCES += tst_qquickopenglinfo.cpp

TESTDATA = data/*
include(../../shared/util.pri)

osx:CONFIG -= app_bundle

CONFIG += parallel_test
QT += quick testlib

OTHER_FILES += \
    data/basic.qml

