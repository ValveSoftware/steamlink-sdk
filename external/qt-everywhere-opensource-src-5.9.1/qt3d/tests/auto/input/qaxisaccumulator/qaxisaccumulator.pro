TEMPLATE = app

TARGET = tst_qaxisaccumulator

QT += core-private 3dcore 3dcore-private 3dinput 3dinput-private testlib

CONFIG += testcase

SOURCES += tst_qaxisaccumulator.cpp

include(../commons/commons.pri)
