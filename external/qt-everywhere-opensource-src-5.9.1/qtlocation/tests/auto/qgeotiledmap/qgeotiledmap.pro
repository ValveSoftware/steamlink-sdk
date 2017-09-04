TEMPLATE = app
CONFIG += testcase
TARGET = tst_qgeotiledmap
INCLUDEPATH += ../geotestplugin

SOURCES += tst_qgeotiledmap.cpp

QT += location-private positioning-private testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
