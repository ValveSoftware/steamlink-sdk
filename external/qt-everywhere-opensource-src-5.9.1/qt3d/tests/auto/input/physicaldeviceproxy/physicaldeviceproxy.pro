TEMPLATE = app

TARGET = tst_physicaldeviceproxy

QT += 3dcore 3dcore-private 3dinput 3dinput-private testlib

CONFIG += testcase

SOURCES += tst_physicaldeviceproxy.cpp

include(../commons/commons.pri)
