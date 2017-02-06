TEMPLATE = app

TARGET = tst_qdefaultmeshes

QT += core-private 3dcore 3dcore-private 3drender 3drender-private testlib 3dextras

CONFIG += testcase

SOURCES += \
    tst_qdefaultmeshes.cpp

include(../commons/commons.pri)
