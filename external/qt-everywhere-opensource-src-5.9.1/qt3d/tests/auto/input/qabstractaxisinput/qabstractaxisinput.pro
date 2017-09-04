TEMPLATE = app

TARGET = tst_qabstractaxisinput

QT += core-private 3dcore 3dcore-private 3dinput 3dinput-private testlib

CONFIG += testcase

SOURCES += tst_qabstractaxisinput.cpp

include(../commons/commons.pri)
