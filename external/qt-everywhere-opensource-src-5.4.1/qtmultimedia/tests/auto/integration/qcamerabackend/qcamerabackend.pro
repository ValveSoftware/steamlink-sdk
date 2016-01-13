TARGET = tst_qcamerabackend

# DirectShow plugin requires widgets.
QT += multimedia-private widgets testlib

# This is more of a system test
CONFIG += testcase

SOURCES += tst_qcamerabackend.cpp
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0

