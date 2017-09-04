TEMPLATE = app

TARGET = tst_lerpclipblend

QT += 3dcore 3dcore-private 3danimation 3danimation-private testlib

CONFIG += testcase

SOURCES += tst_lerpclipblend.cpp

include(../../core/common/common.pri)
