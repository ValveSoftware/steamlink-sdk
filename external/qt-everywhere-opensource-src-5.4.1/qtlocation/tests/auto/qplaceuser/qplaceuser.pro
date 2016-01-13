TEMPLATE = app
CONFIG += testcase
TARGET = tst_qplaceuser

SOURCES += tst_qplaceuser.cpp

QT += location testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
