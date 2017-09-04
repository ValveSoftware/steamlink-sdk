TEMPLATE = app

TARGET = tst_qattribute

QT += core-private 3dcore 3dcore-private 3drender 3drender-private testlib

CONFIG += testcase

SOURCES += tst_qattribute.cpp

include(../../core/common/common.pri)
include(../commons/commons.pri)
