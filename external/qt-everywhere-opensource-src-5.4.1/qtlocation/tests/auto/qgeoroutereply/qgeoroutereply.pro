TEMPLATE = app
CONFIG+=testcase
TARGET=tst_qgeoroutereply

# Input
HEADERS += tst_qgeoroutereply.h
SOURCES += tst_qgeoroutereply.cpp

QT += location testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
