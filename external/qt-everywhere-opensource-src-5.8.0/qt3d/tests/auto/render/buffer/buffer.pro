TEMPLATE = app

TARGET = tst_buffer

QT += 3dcore 3dcore-private 3drender 3drender-private testlib

CONFIG += testcase

SOURCES += tst_buffer.cpp

include(../../core/common/common.pri)
include(../commons/commons.pri)
