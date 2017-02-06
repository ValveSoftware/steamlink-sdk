TEMPLATE = app

TARGET = tst_sceneloader

QT += 3dcore 3dcore-private 3drender 3drender-private testlib

CONFIG += testcase

SOURCES += tst_sceneloader.cpp

include(../../core/common/common.pri)
include(../commons/commons.pri)
