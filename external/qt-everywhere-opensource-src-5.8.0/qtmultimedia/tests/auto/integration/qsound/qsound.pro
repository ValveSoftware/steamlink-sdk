TARGET = tst_qsound

QT += core multimedia-private testlib

# This is more of a system test
CONFIG += testcase

SOURCES += tst_qsound.cpp

TESTDATA += test.wav
