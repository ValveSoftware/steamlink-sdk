TEMPLATE = app

TARGET = tst_effect

QT += 3dcore 3dcore-private 3drender 3drender-private testlib

CONFIG += testcase

SOURCES += tst_effect.cpp

include(../../core/common/common.pri)
include(../commons/commons.pri)
