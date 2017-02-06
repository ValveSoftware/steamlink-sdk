TEMPLATE = app

TARGET = tst_texturedatamanager

QT += 3dcore 3dcore-private 3drender 3drender-private testlib

CONFIG += testcase

SOURCES += tst_texturedatamanager.cpp

include(../../core/common/common.pri)
include(../commons/commons.pri)
