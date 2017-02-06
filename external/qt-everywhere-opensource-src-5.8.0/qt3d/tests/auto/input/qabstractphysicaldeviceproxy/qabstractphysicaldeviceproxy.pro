TEMPLATE = app

TARGET = tst_qabstractphysicaldeviceproxy

QT += 3dcore 3dcore-private 3dinput 3dinput-private testlib

CONFIG += testcase

SOURCES += tst_qabstractphysicaldeviceproxy.cpp

include(../commons/commons.pri)
