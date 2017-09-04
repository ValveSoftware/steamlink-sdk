TEMPLATE = app

TARGET = tst_animationutils

QT += 3dcore 3dcore-private 3danimation 3danimation-private testlib

CONFIG += testcase

SOURCES += \
    tst_animationutils.cpp

include(../../core/common/common.pri)

RESOURCES += \
    animationutils.qrc
