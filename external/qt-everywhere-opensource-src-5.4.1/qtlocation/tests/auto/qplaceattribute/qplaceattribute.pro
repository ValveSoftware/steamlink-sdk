TEMPLATE = app
CONFIG += testcase
TARGET = tst_qplaceattribute

SOURCES += tst_qplaceattribute.cpp

QT += location testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
