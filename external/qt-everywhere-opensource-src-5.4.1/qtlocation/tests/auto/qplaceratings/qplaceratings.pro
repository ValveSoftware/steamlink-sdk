TEMPLATE = app
CONFIG += testcase
TARGET = tst_qplaceratings

SOURCES += tst_qplaceratings.cpp

QT += location testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
