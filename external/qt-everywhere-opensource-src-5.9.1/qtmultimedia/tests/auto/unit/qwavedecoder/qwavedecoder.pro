TARGET = tst_qwavedecoder
HEADERS += ../../../../src/multimedia/audio/qwavedecoder_p.h
SOURCES += tst_qwavedecoder.cpp \
           ../../../../src/multimedia/audio/qwavedecoder_p.cpp

QT += multimedia-private testlib network
CONFIG += testcase

TESTDATA += data/*
