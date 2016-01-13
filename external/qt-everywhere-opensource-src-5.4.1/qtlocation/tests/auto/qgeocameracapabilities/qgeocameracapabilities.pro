TEMPLATE = app
CONFIG += testcase
TARGET = tst_qgeocameracapabilities

INCLUDEPATH += ../../../src/location/maps

SOURCES += tst_qgeocameracapabilities.cpp

QT += location positioning-private testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
