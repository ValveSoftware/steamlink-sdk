TEMPLATE = app

TARGET = tst_qrendertargetoutput

QT += 3dcore 3dcore-private 3drender 3drender-private testlib

CONFIG += testcase

SOURCES += tst_qrendertargetoutput.cpp

include(../../core/common/common.pri)
include(../commons/commons.pri)
