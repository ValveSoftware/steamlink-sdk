TARGET = tst_qaudiodecoderbackend

QT += multimedia multimedia-private testlib

# This is more of a system test
CONFIG += testcase insignificant_test
TESTDATA += testdata/*

INCLUDEPATH += \
    ../../../../src/multimedia/audio

SOURCES += \
    tst_qaudiodecoderbackend.cpp
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
