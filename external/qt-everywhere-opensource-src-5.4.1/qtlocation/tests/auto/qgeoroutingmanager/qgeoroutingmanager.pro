TEMPLATE = app
CONFIG+=testcase
TARGET=tst_qgeoroutingmanager

HEADERS += tst_qgeoroutingmanager.h

SOURCES += tst_qgeoroutingmanager.cpp

QT += location testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
