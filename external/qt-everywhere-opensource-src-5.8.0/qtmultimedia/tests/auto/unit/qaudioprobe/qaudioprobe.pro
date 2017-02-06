CONFIG += testcase
TARGET = tst_qaudioprobe

QT += multimedia-private testlib

SOURCES += tst_qaudioprobe.cpp

include (../qmultimedia_common/mock.pri)
include (../qmultimedia_common/mockrecorder.pri)

