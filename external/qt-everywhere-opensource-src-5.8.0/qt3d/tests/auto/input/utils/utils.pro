TEMPLATE = app

TARGET = tst_utils

QT += 3dcore 3dcore-private 3dinput 3dinput-private testlib

CONFIG += testcase

SOURCES += tst_utils.cpp

include(../commons/commons.pri)
