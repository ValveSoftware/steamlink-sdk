TEMPLATE = app

TARGET = tst_clipblendvalue

QT += 3dcore 3dcore-private 3danimation 3danimation-private testlib

CONFIG += testcase

SOURCES += \
    tst_clipblendvalue.cpp

include(../../core/common/common.pri)
