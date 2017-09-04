TEMPLATE = app

TARGET = tst_qgeometryrenderer

QT += core-private 3dcore 3dcore-private 3drender 3drender-private testlib

CONFIG += testcase

SOURCES += tst_qgeometryrenderer.cpp

include(../../core/common/common.pri)
