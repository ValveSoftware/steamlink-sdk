TEMPLATE = app

TARGET = tst_qlerpclipblend

QT += 3dcore 3dcore-private 3danimation 3danimation-private testlib

CONFIG += testcase

SOURCES += \
    tst_qlerpclipblend.cpp

include(../../core/common/common.pri)
