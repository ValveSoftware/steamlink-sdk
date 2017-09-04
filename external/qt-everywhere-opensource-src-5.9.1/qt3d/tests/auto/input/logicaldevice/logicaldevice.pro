TEMPLATE = app

TARGET = tst_logicaldevice

QT += 3dcore 3dcore-private 3dinput 3dinput-private testlib

CONFIG += testcase

SOURCES += tst_logicaldevice.cpp

include(../../core/common/common.pri)
