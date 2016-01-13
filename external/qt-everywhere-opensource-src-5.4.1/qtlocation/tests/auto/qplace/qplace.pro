TEMPLATE = app
CONFIG += testcase
TARGET = tst_qplace

SOURCES += tst_qplace.cpp

QT += location testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
