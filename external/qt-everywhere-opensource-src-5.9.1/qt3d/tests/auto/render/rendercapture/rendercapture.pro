TEMPLATE = app

TARGET = rendercapture

QT += 3dcore 3dcore-private 3drender 3drender-private testlib

CONFIG += testcase

SOURCES += tst_rendercapture.cpp

include(../../core/common/common.pri)
include(../commons/commons.pri)
