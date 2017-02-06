TEMPLATE = app

TARGET = tst_qrendersurfaceselector
QT += core-private 3dcore 3dcore-private 3drender 3drender-private testlib

CONFIG += testcase

SOURCES += tst_qrendersurfaceselector.cpp

include(../../core/common/common.pri)
include(../commons/commons.pri)
