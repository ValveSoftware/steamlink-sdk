TEMPLATE = app

TARGET = tst_qframegraphnode
QT += core-private 3dcore 3dcore-private 3drender 3drender-private testlib

CONFIG += testcase

SOURCES += tst_qframegraphnode.cpp

include(../../core/common/common.pri)
