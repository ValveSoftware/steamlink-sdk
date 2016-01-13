CONFIG += testcase
TARGET = tst_qcameraimagecapture

QT += multimedia-private testlib

SOURCES += \
    tst_qcameraimagecapture.cpp

include (../qmultimedia_common/mock.pri)
include (../qmultimedia_common/mockcamera.pri)
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
