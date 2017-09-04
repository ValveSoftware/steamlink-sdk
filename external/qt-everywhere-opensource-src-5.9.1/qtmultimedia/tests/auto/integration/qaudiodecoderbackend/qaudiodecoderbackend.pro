TARGET = tst_qaudiodecoderbackend

QT += multimedia multimedia-private testlib

# This is more of a system test
CONFIG += testcase
TESTDATA += testdata/*

INCLUDEPATH += \
    ../../../../src/multimedia/audio

SOURCES += \
    tst_qaudiodecoderbackend.cpp
