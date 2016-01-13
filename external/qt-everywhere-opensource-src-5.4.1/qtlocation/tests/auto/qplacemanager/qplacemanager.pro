TEMPLATE = app
CONFIG += testcase
TARGET = tst_qplacemanager

SOURCES += tst_qplacemanager.cpp

QT += location testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
