TEMPLATE = app

TARGET = tst_qadditiveclipblend

QT += 3dcore 3dcore-private 3danimation 3danimation-private testlib

CONFIG += testcase

SOURCES += \
    tst_qadditiveclipblend.cpp

include(../../core/common/common.pri)
