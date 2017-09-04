TEMPLATE = app

TARGET = tst_filterentitybycomponent

QT += core-private 3dcore 3dcore-private 3drender 3drender-private testlib

CONFIG += testcase

SOURCES += tst_filterentitybycomponent.cpp

CONFIG += useCommonTestAspect

include(../commons/commons.pri)
