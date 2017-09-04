TEMPLATE = app

TARGET = tst_ddstextures

CONFIG += testcase

SOURCES += tst_ddstextures.cpp

OTHER_FILES = \
    data/16x16x1-1-bc1.dds \
    data/16x16x1-1-bc1-dx10.dds \
    data/16x16x1-1-bc1-nomips.dds \
    data/16x16x1-1-bc1-nomips-dx10.dds \
    data/16x16x1-1-bc3.dds \
    data/16x16x1-1-bc3-dx10.dds \
    data/16x16x1-1-bc3-nomips.dds \
    data/16x16x1-1-bc3-nomips-dx10.dds \
    data/16x16x1-1-lumi.dds \
    data/16x16x1-1-lumi-nomips.dds \
    data/16x16x1-1-rgb.dds \
    data/16x16x1-1-rgb-nomips.dds \
    data/16x16x1-6-bc1.dds \
    data/16x16x1-6-bc1-dx10.dds \
    data/16x16x1-6-bc1-nomips.dds \
    data/16x16x1-6-bc1-nomips-dx10.dds \
    data/16x16x1-6-bc3.dds \
    data/16x16x1-6-bc3-dx10.dds \
    data/16x16x1-6-bc3-nomips.dds \
    data/16x16x1-6-bc3-nomips-dx10.dds \
    data/16x16x1-6-lumi.dds \
    data/16x16x1-6-lumi-nomips.dds \
    data/16x16x1-6-rgb.dds \
    data/16x16x1-6-rgb-nomips.dds

TESTDATA = data/*

QT += core-private 3dcore 3dcore-private 3drender 3drender-private testlib
