TARGET = tst_qaudioinput

QT += core multimedia-private testlib

# This is more of a system test
CONFIG += testcase insignificant_test

HEADERS += wavheader.h
SOURCES += wavheader.cpp tst_qaudioinput.cpp
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
