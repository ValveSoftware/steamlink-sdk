TEMPLATE = app

TARGET = tst_loadscenejob

QT += 3dcore 3dcore-private 3drender 3drender-private testlib

CONFIG += testcase

SOURCES += tst_loadscenejob.cpp

include(../../core/common/common.pri)
include(../commons/commons.pri)
