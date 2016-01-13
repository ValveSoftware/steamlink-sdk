TARGET = tst_qwavedecoder
HEADERS += ../../../../src/multimedia/audio/qwavedecoder_p.h
SOURCES += tst_qwavedecoder.cpp \
           ../../../../src/multimedia/audio/qwavedecoder_p.cpp

QT += multimedia-private testlib network
CONFIG += no_private_qt_headers_warning testcase

TESTDATA += data/*
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
