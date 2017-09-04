TEMPLATE = app

TARGET = tst_animationclip

QT += core-private 3dcore 3dcore-private 3danimation 3danimation-private testlib

CONFIG += testcase

SOURCES += \
    tst_animationclip.cpp

include(../../core/common/common.pri)
