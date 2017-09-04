TEMPLATE = app

TARGET = tst_clipblendnodevisitor

QT += 3dcore 3dcore-private 3danimation 3danimation-private testlib

CONFIG += testcase

SOURCES += tst_clipblendnodevisitor.cpp

include(../../core/common/common.pri)
