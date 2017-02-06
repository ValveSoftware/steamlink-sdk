CONFIG += testcase
TARGET = tst_qquickgraphicsinfo
SOURCES += tst_qquickgraphicsinfo.cpp

TESTDATA = data/*
include(../../shared/util.pri)

osx:CONFIG -= app_bundle

QT += quick testlib

OTHER_FILES += \
    data/basic.qml

