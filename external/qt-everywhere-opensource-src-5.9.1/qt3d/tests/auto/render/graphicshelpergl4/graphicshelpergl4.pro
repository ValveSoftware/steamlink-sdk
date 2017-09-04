TEMPLATE = app

TARGET = tst_graphicshelpergl4

QT += 3dcore 3dcore-private 3drender 3drender-private testlib

CONFIG += testcase

SOURCES += \
    tst_graphicshelpergl4.cpp

include(../../core/common/common.pri)
include(../commons/commons.pri)
