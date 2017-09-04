TEMPLATE = app

TARGET = boundingvolumedebug

QT += 3dcore 3dcore-private 3drender 3drender-private testlib

CONFIG += testcase

SOURCES += tst_boundingvolumedebug.cpp

include(../commons/commons.pri)
