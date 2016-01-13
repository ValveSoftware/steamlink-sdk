TARGET = tst_qmediaplayerbackend

QT += multimedia-private testlib

# This is more of a system test
CONFIG += testcase insignificant_test


SOURCES += \
    tst_qmediaplayerbackend.cpp

TESTDATA += testdata/*
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
