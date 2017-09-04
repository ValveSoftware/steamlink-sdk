TEMPLATE = app

TARGET = tst_qrendercapture

QT += core-private 3dcore 3dcore-private 3drender 3drender-private testlib

CONFIG += testcase

SOURCES += tst_qrendercapture.cpp

include(../commons/commons.pri)
include(../../core/common/common.pri)
