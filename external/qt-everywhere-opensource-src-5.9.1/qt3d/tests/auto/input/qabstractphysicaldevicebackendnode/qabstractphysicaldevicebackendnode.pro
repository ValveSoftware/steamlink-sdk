TEMPLATE = app

TARGET = tst_qabstractphysicaldevicebackendnode

QT += 3dcore 3dcore-private 3dinput 3dinput-private testlib

CONFIG += testcase

SOURCES += tst_qabstractphysicaldevicebackendnode.cpp

include(../commons/commons.pri)
