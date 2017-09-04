TEMPLATE = app

TARGET = tst_qrendertargetselector
QT += core-private 3dcore 3dcore-private 3drender 3drender-private testlib

CONFIG += testcase

SOURCES += tst_qrendertargetselector.cpp

include(../../core/common/common.pri)
