TEMPLATE = app

TARGET = tst_qparameter

QT += 3dcore 3dcore-private 3drender 3drender-private testlib

CONFIG += testcase

SOURCES += tst_qparameter.cpp

include(../../core/common/common.pri)
