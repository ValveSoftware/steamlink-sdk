TEMPLATE = app

TARGET = tst_bench_materialparametergathering

QT += core-private 3dcore 3dcore-private 3drender 3drender-private 3dextras testlib

CONFIG += testcase

SOURCES += tst_bench_materialparametergathering.cpp

include(../../../auto/render/commons/commons.pri)

# Needed to use the TestAspect
DEFINES += QT_BUILD_INTERNAL
