TEMPLATE = app
CONFIG += testcase
TARGET = tst_maptype

SOURCES += tst_maptype.cpp

QT += location-private testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
