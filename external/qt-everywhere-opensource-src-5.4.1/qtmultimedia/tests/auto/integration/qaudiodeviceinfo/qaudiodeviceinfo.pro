TARGET = tst_qaudiodeviceinfo

QT += core multimedia-private testlib

# This is more of a system test
CONFIG += testcase insignificant_test

SOURCES += tst_qaudiodeviceinfo.cpp

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
