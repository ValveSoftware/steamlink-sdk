TEMPLATE = app
CONFIG+=testcase
TARGET=tst_qgeorouterequest

# Input
HEADERS += tst_qgeorouterequest.h
SOURCES += tst_qgeorouterequest.cpp

QT += location testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
