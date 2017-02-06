TEMPLATE = app

TARGET = tst_qgraphicsapifilter

QT += 3dcore 3dcore-private 3drender 3drender-private testlib

CONFIG += testcase

SOURCES += \
    tst_qgraphicsapifilter.cpp

include(../../core/common/common.pri)
include(../commons/commons.pri)
