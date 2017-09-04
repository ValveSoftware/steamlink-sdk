CONFIG += testcase
TARGET = tst_drawingmodes
SOURCES += tst_drawingmodes.cpp

macos:CONFIG -= app_bundle

TESTDATA = data/*

include(../../shared/util.pri)

CONFIG += parallel_test
QT += gui qml quick testlib

OTHER_FILES += \
    data/DrawingModes.qml

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
