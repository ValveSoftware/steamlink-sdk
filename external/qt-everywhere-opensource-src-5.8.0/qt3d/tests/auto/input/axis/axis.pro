TEMPLATE = app

TARGET = tst_axis

QT += core-private 3dcore 3dcore-private 3dinput 3dinput-private testlib

CONFIG += testcase

SOURCES += tst_axis.cpp

include(../../core/common/common.pri)
