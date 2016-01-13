TEMPLATE = app
CONFIG += testcase
TARGET = tst_doublevectors

SOURCES += tst_doublevectors.cpp

QT += positioning-private testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
