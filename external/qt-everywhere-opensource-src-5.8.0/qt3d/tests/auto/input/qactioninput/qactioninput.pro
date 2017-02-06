TEMPLATE = app

TARGET = tst_qactioninput

QT += core-private 3dcore 3dcore-private 3dinput 3dinput-private testlib

CONFIG += testcase

SOURCES += tst_qactioninput.cpp

include(../commons/commons.pri)
