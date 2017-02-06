TEMPLATE = app

TARGET = tst_bench_layerfiltering

QT += core-private 3dcore 3dcore-private 3drender 3drender-private testlib

CONFIG += testcase

SOURCES += tst_bench_layerfiltering.cpp

include(../../../auto/render/commons/commons.pri)

# Needed to use the TestAspect
DEFINES += QT_BUILD_INTERNAL
