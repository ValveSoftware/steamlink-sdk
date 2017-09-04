TEMPLATE = app

TARGET = tst_clipblendnode

QT += 3dcore 3dcore-private 3danimation 3danimation-private testlib

CONFIG += testcase

SOURCES += tst_clipblendnode.cpp

include(../../core/common/common.pri)
