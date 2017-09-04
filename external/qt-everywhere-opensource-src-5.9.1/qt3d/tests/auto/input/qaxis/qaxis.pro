TEMPLATE = app

TARGET = tst_qaxis

QT += core-private 3dcore 3dcore-private 3dinput 3dinput-private testlib

CONFIG += testcase

SOURCES += tst_qaxis.cpp

include(../../core/common/common.pri)
