TEMPLATE = app

TARGET = tst_qeffect

QT += 3dcore 3dcore-private 3drender 3drender-private testlib

CONFIG += testcase

SOURCES += tst_qeffect.cpp

include(../../core/common/common.pri)
include(../commons/commons.pri)
