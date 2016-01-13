TEMPLATE = app
CONFIG += testcase
TARGET = tst_qgeocameradata

INCLUDEPATH += ../../../src/location/maps

SOURCES += tst_qgeocameradata.cpp

QT += location positioning-private testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
