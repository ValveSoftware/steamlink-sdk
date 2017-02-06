TEMPLATE = app

TARGET = tst_qrendertarget

QT += 3dcore 3dcore-private 3drender 3drender-private testlib

CONFIG += testcase

SOURCES += tst_qrendertarget.cpp

include(../../core/common/common.pri)
include(../commons/commons.pri)
