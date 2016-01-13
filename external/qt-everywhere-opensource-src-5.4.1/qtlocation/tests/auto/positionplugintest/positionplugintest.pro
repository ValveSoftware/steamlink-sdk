TEMPLATE = app
CONFIG+=testcase
TARGET=tst_positionplugin

SOURCES += tst_positionplugin.cpp

QT += positioning testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
