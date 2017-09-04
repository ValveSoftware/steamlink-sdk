TEMPLATE = app

TARGET = tst_qlogicaldevice

QT += core-private 3dcore 3dcore-private 3dinput 3dinput-private testlib

CONFIG += testcase

SOURCES += tst_qlogicaldevice.cpp

include(../../core/common/common.pri)
