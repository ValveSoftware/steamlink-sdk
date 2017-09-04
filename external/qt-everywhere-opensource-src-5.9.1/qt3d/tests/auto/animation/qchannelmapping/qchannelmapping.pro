TEMPLATE = app

TARGET = tst_qchannelmapping

QT += 3dcore 3dcore-private 3danimation 3danimation-private testlib

CONFIG += testcase

SOURCES += \
    tst_qchannelmapping.cpp

include(../../core/common/common.pri)
