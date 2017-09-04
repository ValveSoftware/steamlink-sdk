TEMPLATE = app

TARGET = tst_qpostman

QT += 3dcore 3dcore-private 3drender 3drender-private testlib

CONFIG += testcase

SOURCES += tst_qpostman.cpp

include(../common/common.pri)
