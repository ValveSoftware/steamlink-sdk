TEMPLATE = app

TARGET = tst_qblendedclipanimator

QT += 3dcore 3dcore-private 3danimation 3danimation-private testlib

CONFIG += testcase

SOURCES += \
    tst_qblendedclipanimator.cpp

include(../../core/common/common.pri)
