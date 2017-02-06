TEMPLATE = app

TARGET = tst_entity

QT += 3dcore 3dcore-private 3drender 3drender-private testlib

CONFIG += testcase

SOURCES += tst_entity.cpp

include(../commons/commons.pri)
