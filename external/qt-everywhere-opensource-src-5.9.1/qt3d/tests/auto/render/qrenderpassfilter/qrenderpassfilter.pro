TEMPLATE = app

TARGET = tst_qrenderpassfilter
QT += core-private 3dcore 3dcore-private 3drender 3drender-private testlib

CONFIG += testcase

SOURCES += tst_qrenderpassfilter.cpp

include(../../core/common/common.pri)
