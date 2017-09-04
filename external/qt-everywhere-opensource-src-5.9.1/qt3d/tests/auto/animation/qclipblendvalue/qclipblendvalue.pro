TEMPLATE = app

TARGET = tst_qclipblendvalue

QT += 3dcore 3dcore-private 3danimation 3danimation-private testlib

CONFIG += testcase

SOURCES += \
    tst_qclipblendvalue.cpp

include(../../core/common/common.pri)
