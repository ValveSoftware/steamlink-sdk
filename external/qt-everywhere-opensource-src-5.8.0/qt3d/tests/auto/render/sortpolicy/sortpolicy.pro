TEMPLATE = app

TARGET = tst_sortpolicy

QT += 3dcore 3dcore-private 3drender 3drender-private testlib

CONFIG += testcase

SOURCES += \
    tst_sortpolicy.cpp

include(../../core/common/common.pri)
include(../commons/commons.pri)
