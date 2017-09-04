TEMPLATE = app

TARGET = tst_environmentlight

QT += core-private 3dcore 3dcore-private 3drender 3drender-private testlib

CONFIG += testcase

SOURCES += tst_qenvironmentlight.cpp

include(../../core/common/common.pri)
