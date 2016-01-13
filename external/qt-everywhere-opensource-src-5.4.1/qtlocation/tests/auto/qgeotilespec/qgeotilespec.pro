TEMPLATE = app
CONFIG += testcase
TARGET = tst_qgeotilespec

INCLUDEPATH += ../../../src/location/maps

SOURCES += tst_qgeotilespec.cpp

QT += location testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
