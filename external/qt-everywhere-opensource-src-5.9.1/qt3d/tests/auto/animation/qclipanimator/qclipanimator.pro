TEMPLATE = app

TARGET = tst_qclipanimator

QT += 3dcore 3dcore-private 3danimation 3danimation-private testlib

CONFIG += testcase

SOURCES += \
    tst_qclipanimator.cpp

include(../../core/common/common.pri)
