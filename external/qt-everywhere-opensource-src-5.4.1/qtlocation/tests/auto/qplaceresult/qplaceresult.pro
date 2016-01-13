TEMPLATE = app
CONFIG += testcase
TARGET = tst_qplaceresult

SOURCES += tst_qplaceresult.cpp

QT += location testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
