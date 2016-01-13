TEMPLATE = app
CONFIG += testcase
TARGET = tst_qplaceimage

SOURCES += tst_qplaceimage.cpp

QT += location testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
