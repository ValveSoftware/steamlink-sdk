TEMPLATE = app
CONFIG+=testcase
TARGET=tst_qgeoroute

# Input
HEADERS += tst_qgeoroute.h
SOURCES += tst_qgeoroute.cpp

QT += location testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
