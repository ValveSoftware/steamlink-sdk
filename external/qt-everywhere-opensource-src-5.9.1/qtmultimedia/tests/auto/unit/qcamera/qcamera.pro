CONFIG += testcase
TARGET = tst_qcamera

QT += multimedia-private testlib

include (../qmultimedia_common/mock.pri)
include (../qmultimedia_common/mockcamera.pri)

SOURCES += tst_qcamera.cpp
