TEMPLATE = app

TARGET = tst_mousedevice

QT += 3dcore 3dcore-private 3dinput 3dinput-private testlib

CONFIG += testcase

SOURCES += tst_mousedevice.cpp

include(../../core/common/common.pri)
