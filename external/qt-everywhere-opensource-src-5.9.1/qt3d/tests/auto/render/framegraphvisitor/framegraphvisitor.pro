TEMPLATE = app

TARGET = tst_framegraphvisitor

QT += core-private 3dcore 3dcore-private 3drender 3drender-private testlib

CONFIG += testcase

SOURCES += tst_framegraphvisitor.cpp

CONFIG += useCommonTestAspect

include(../commons/commons.pri)
