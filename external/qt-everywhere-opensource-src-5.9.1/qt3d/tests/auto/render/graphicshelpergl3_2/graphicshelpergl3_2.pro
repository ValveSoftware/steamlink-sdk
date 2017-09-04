TEMPLATE = app

TARGET = tst_graphicshelpergl3_2

QT += 3dcore 3dcore-private 3drender 3drender-private testlib

CONFIG += testcase

SOURCES += \
    tst_graphicshelpergl3_2.cpp

include(../../core/common/common.pri)
include(../commons/commons.pri)
