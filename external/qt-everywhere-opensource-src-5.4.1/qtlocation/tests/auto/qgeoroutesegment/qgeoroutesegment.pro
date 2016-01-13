TEMPLATE = app
CONFIG+=testcase
TARGET=tst_qgeoroutesegment

# Input
HEADERS += tst_qgeoroutesegment.h
SOURCES += tst_qgeoroutesegment.cpp

QT += location testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
